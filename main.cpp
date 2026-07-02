#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>
#include <functional>
#include <cerrno>
#include <termios.h>
#include <signal.h>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <unordered_set>
#include <fcntl.h>
#include <glob.h>
#include <cstring>
#include "header/command.h"

// Token struct
struct Token {
    std::string text;
    bool is_quoted;
};

// Command info struct
struct CommandInfo {
    std::vector<std::string> argv;
    std::string stdin_file;
    std::string stdout_file;
    bool stdout_append = false;
};

// Forward declaration of TerminalReader for signal handler
class TerminalReader;
static TerminalReader* global_reader = nullptr;


static std::string expandTilde(const std::string& path) {
    if (path.empty()) return path;
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        std::string homeStr;
        if (home) {
            homeStr = home;
        } else {
            struct passwd* pw = getpwuid(geteuid());
            if (pw) {
                homeStr = pw->pw_dir;
            }
        }
        if (path.size() == 1) {
            return homeStr;
        }
        if (path[1] == '/') {
            return homeStr + path.substr(1);
        }
    }
    return path;
}

static std::vector<std::string> expandGlob(const std::string& pattern) {
    std::vector<std::string> results;
    if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
        results.push_back(pattern);
        return results;
    }
    glob_t glob_result;
    std::memset(&glob_result, 0, sizeof(glob_result));
    int ret = glob(pattern.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &glob_result);
    if (ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            results.push_back(glob_result.gl_pathv[i]);
        }
    } else {
        results.push_back(pattern);
    }
    globfree(&glob_result);
    return results;
}

static std::vector<Token> parseTokens(const std::string& line) {
    std::vector<Token> tokens;
    std::string current;
    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;
    bool token_quoted = false;

    auto commit_token = [&]() {
        if (!current.empty() || token_quoted) {
            std::string text = current;
            if (!token_quoted) {
                text = expandTilde(text);
            }
            tokens.push_back({text, token_quoted});
            current.clear();
            token_quoted = false;
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            current += c;
            escaped = false;
        } else if (c == '\\') {
            if (in_single_quotes) {
                current += c;
            } else {
                escaped = true;
            }
        } else if (c == '"') {
            if (in_single_quotes) {
                current += c;
            } else {
                in_double_quotes = !in_double_quotes;
                token_quoted = true;
            }
        } else if (c == '\'') {
            if (in_double_quotes) {
                current += c;
            } else {
                in_single_quotes = !in_single_quotes;
                token_quoted = true;
            }
        } else if (c == '$' && !in_single_quotes) {
            std::string varName;
            size_t j = i + 1;
            while (j < line.size() && (isalnum(line[j]) || line[j] == '_')) {
                varName += line[j];
                j++;
            }
            if (!varName.empty()) {
                const char* val = getenv(varName.c_str());
                if (val) {
                    current += val;
                }
                i = j - 1;
            } else {
                current += '$';
            }
        } else if (c == ' ' || c == '\t') {
            if (in_double_quotes || in_single_quotes) {
                current += c;
            } else {
                commit_token();
            }
        } else {
            current += c;
        }
    }
    commit_token();
    return tokens;
}

static std::vector<std::string> splitPipeline(const std::string& line) {
    std::vector<std::string> segments;
    std::string current;
    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            current += c;
            escaped = false;
        } else if (c == '\\') {
            current += c;
            escaped = true;
        } else if (c == '"') {
            if (in_single_quotes) {
                current += c;
            } else {
                in_double_quotes = !in_double_quotes;
                current += c;
            }
        } else if (c == '\'') {
            if (in_double_quotes) {
                current += c;
            } else {
                in_single_quotes = !in_single_quotes;
                current += c;
            }
        } else if (c == '|' && !in_double_quotes && !in_single_quotes) {
            segments.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty() || segments.empty()) {
        segments.push_back(current);
    }
    return segments;
}

static CommandInfo parseCommandInfo(const std::string& segment) {
    std::vector<Token> tokens = parseTokens(segment);
    CommandInfo info;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!tokens[i].is_quoted && tokens[i].text == "<") {
            if (i + 1 < tokens.size()) {
                info.stdin_file = tokens[i + 1].text;
                i++;
            }
        } else if (!tokens[i].is_quoted && tokens[i].text == ">") {
            if (i + 1 < tokens.size()) {
                info.stdout_file = tokens[i + 1].text;
                info.stdout_append = false;
                i++;
            }
        } else if (!tokens[i].is_quoted && tokens[i].text == ">>") {
            if (i + 1 < tokens.size()) {
                info.stdout_file = tokens[i + 1].text;
                info.stdout_append = true;
                i++;
            }
        } else {
            if (!tokens[i].is_quoted) {
                std::vector<std::string> expanded = expandGlob(tokens[i].text);
                info.argv.insert(info.argv.end(), expanded.begin(), expanded.end());
            } else {
                info.argv.push_back(tokens[i].text);
            }
        }
    }
    return info;
}

static std::string longestCommonPrefix(const std::vector<std::string>& strs) {
    if (strs.empty()) return "";
    std::string prefix = strs[0];
    for (size_t i = 1; i < strs.size(); ++i) {
        while (strs[i].find(prefix) != 0) {
            prefix = prefix.substr(0, prefix.length() - 1);
            if (prefix.empty()) return "";
        }
    }
    return prefix;
}

class TerminalReader {
private:
    struct termios orig_termios;
    bool raw_mode_enabled = false;

public:
    TerminalReader() = default;

    void enableRawMode() {
        if (raw_mode_enabled) return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;

        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
        raw_mode_enabled = true;
    }

    void disableRawMode() {
        if (!raw_mode_enabled) return;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }

    ~TerminalReader() {
        disableRawMode();
    }

    std::string readLine(const std::string& prompt, const std::vector<std::string>& history, bool& should_exit) {
        enableRawMode();

        std::string current_input;
        std::string temp_input;
        size_t cursor_pos = 0;
        size_t history_index = history.size();

        auto redraw = [&]() {
            std::cout << "\r" << prompt << current_input << "\033[K";
            std::cout << "\r" << prompt << current_input.substr(0, cursor_pos) << std::flush;
        };

        std::cout << prompt << std::flush;

        while (true) {
            char c;
            int nread = read(STDIN_FILENO, &c, 1);
            if (nread <= 0) {
                should_exit = true;
                disableRawMode();
                return "";
            }

            if (c == '\r' || c == '\n') {
                std::cout << "\r\n";
                disableRawMode();
                return current_input;
            } else if (c == 127 || c == 8) {
                if (cursor_pos > 0) {
                    current_input.erase(cursor_pos - 1, 1);
                    cursor_pos--;
                    redraw();
                }
            } else if (c == '\t') { // Tab Completion
                size_t start = cursor_pos;
                while (start > 0 && current_input[start - 1] != ' ' && current_input[start - 1] != '\t' && current_input[start - 1] != '|') {
                    start--;
                }
                std::string word = current_input.substr(start, cursor_pos - start);
                
                std::vector<std::string> matches;
                bool is_command = (start == 0);
                if (!is_command) {
                    bool only_spaces = true;
                    for (size_t i = 0; i < start; ++i) {
                        if (current_input[i] != ' ' && current_input[i] != '\t') {
                            only_spaces = false;
                            break;
                        }
                    }
                    if (only_spaces) {
                        is_command = true;
                    }
                }

                if (is_command) {
                    static const std::vector<std::string> builtins = {
                        "exit", "echo", "pwd", "ls", "cd", "mkdir", "rm", "touch", "cp", "mv", 
                        "chmod", "chown", "chgrp", "cat", "history", "version", "whoami", 
                        "uname", "date", "help", "clear", "env", "export", "alias", "unalias", 
                        "source", "which", "kill", "grep", "head", "tail", "find", "free", "uptime"
                    };
                    for (const auto& b : builtins) {
                        if (b.find(word) == 0) {
                            matches.push_back(b);
                        }
                    }
                    
                    const char* pathEnv = getenv("PATH");
                    if (pathEnv) {
                        std::string pathStr(pathEnv);
                        std::stringstream ss(pathStr);
                        std::string dir;
                        while (std::getline(ss, dir, ':')) {
                            try {
                                namespace fs = std::filesystem;
                                if (fs::exists(dir) && fs::is_directory(dir)) {
                                    for (const auto& entry : fs::directory_iterator(dir)) {
                                        std::string name = entry.path().filename().string();
                                        if (name.find(word) == 0) {
                                            if (access(entry.path().c_str(), X_OK) == 0) {
                                                if (std::find(matches.begin(), matches.end(), name) == matches.end()) {
                                                    matches.push_back(name);
                                                }
                                            }
                                        }
                                    }
                                }
                            } catch (...) {}
                        }
                    }
                } else {
                    std::string dir_path = ".";
                    std::string prefix = word;
                    size_t last_slash = word.find_last_of('/');
                    if (last_slash != std::string::npos) {
                        dir_path = word.substr(0, last_slash + 1);
                        prefix = word.substr(last_slash + 1);
                        dir_path = expandTilde(dir_path);
                    }
                    
                    try {
                        namespace fs = std::filesystem;
                        if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                            for (const auto& entry : fs::directory_iterator(dir_path)) {
                                std::string name = entry.path().filename().string();
                                if (name.find(prefix) == 0) {
                                    std::string full_match = name;
                                    if (fs::is_directory(entry.path())) {
                                        full_match += "/";
                                    }
                                    if (last_slash != std::string::npos) {
                                        matches.push_back(word.substr(0, last_slash + 1) + full_match);
                                    } else {
                                        matches.push_back(full_match);
                                    }
                                }
                            }
                        }
                    } catch (...) {}
                }

                if (!matches.empty()) {
                    if (matches.size() == 1) {
                        std::string match = matches[0];
                        current_input.replace(start, cursor_pos - start, match);
                        cursor_pos = start + match.size();
                        redraw();
                    } else {
                        std::string lcp = longestCommonPrefix(matches);
                        if (lcp.size() > (cursor_pos - start)) {
                            current_input.replace(start, cursor_pos - start, lcp);
                            cursor_pos = start + lcp.size();
                            redraw();
                        } else {
                            std::cout << "\r\n";
                            std::sort(matches.begin(), matches.end());
                            for (const auto& m : matches) {
                                std::cout << m << "  ";
                            }
                            std::cout << "\r\n";
                            std::cout << prompt << current_input << std::flush;
                            std::cout << "\r" << prompt << current_input.substr(0, cursor_pos) << std::flush;
                        }
                    }
                }
            } else if (c == 3) {
                std::cout << "^C\r\n";
                disableRawMode();
                return "";
            } else if (c == 4) {
                if (current_input.empty()) {
                    std::cout << "exit\r\n";
                    should_exit = true;
                    disableRawMode();
                    return "exit";
                }
            } else if (c == '\033') {
                struct termios raw;
                tcgetattr(STDIN_FILENO, &raw);
                struct termios temp_raw = raw;
                temp_raw.c_cc[VMIN] = 0;
                temp_raw.c_cc[VTIME] = 1;
                tcsetattr(STDIN_FILENO, TCSANOW, &temp_raw);

                char seq[4];
                int n1 = read(STDIN_FILENO, &seq[0], 1);
                int n2 = (n1 > 0) ? read(STDIN_FILENO, &seq[1], 1) : 0;

                tcsetattr(STDIN_FILENO, TCSANOW, &raw);

                if (n1 > 0 && n2 > 0) {
                    if (seq[0] == '[') {
                        if (seq[1] >= '0' && seq[1] <= '9') {
                            tcsetattr(STDIN_FILENO, TCSANOW, &temp_raw);
                            char seq2;
                            int n3 = read(STDIN_FILENO, &seq2, 1);
                            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
                            
                            if (n3 > 0 && seq2 == '~') {
                                if (seq[1] == '3') {
                                    if (cursor_pos < current_input.size()) {
                                        current_input.erase(cursor_pos, 1);
                                        redraw();
                                    }
                                }
                            }
                        } else {
                            switch (seq[1]) {
                                case 'A':
                                    if (!history.empty()) {
                                        if (history_index == history.size()) {
                                            temp_input = current_input;
                                        }
                                        if (history_index > 0) {
                                            history_index--;
                                            current_input = history[history_index];
                                            cursor_pos = current_input.size();
                                            redraw();
                                        }
                                    }
                                    break;
                                case 'B':
                                    if (history_index < history.size()) {
                                        history_index++;
                                        if (history_index == history.size()) {
                                            current_input = temp_input;
                                        } else {
                                            current_input = history[history_index];
                                        }
                                        cursor_pos = current_input.size();
                                        redraw();
                                    }
                                    break;
                                case 'C':
                                    if (cursor_pos < current_input.size()) {
                                        cursor_pos++;
                                        redraw();
                                    }
                                    break;
                                case 'D':
                                    if (cursor_pos > 0) {
                                        cursor_pos--;
                                        redraw();
                                    }
                                    break;
                                case 'H':
                                    cursor_pos = 0;
                                    redraw();
                                    break;
                                case 'F':
                                    cursor_pos = current_input.size();
                                    redraw();
                                    break;
                            }
                        }
                    } else if (seq[0] == 'O') {
                        switch (seq[1]) {
                            case 'H':
                                cursor_pos = 0;
                                redraw();
                                break;
                            case 'F':
                                cursor_pos = current_input.size();
                                redraw();
                                break;
                        }
                    }
                }
            } else if (c >= 32 && c <= 126) {
                current_input.insert(cursor_pos, 1, c);
                cursor_pos++;
                redraw();
            }
        }
    }
};

static void signalHandler(int sig) {
    if (global_reader) {
        global_reader->disableRawMode();
    }
    signal(sig, SIG_DFL);
    raise(sig);
}


static bool isInternal(const std::string& cmd) {
    static const std::unordered_set<std::string> internals = {
        "exit", "echo", "pwd", "ls", "cd", "mkdir", "rm", "touch", "cp", "mv", 
        "chmod", "chown", "chgrp", "cat", "history", "version", "whoami", 
        "uname", "date", "help", "clear", "env", "export", "alias", "unalias", 
        "source", "which", "kill", "grep", "head", "tail", "find", "free", "uptime"
    };
    return internals.count(cmd) > 0;
}

static std::string getSystemUsername() {
    if (const char* login = getlogin()) {
        return std::string(login);
    }
    if (struct passwd* pw = getpwuid(geteuid())) {
        return std::string(pw->pw_name);
    }
    return std::string("unknown");
}

static std::string getSystemHostname() {
    long maxHostName = sysconf(_SC_HOST_NAME_MAX);
    std::size_t bufferSize = (maxHostName > 0 ? static_cast<std::size_t>(maxHostName) : 256);
    std::vector<char> hostname(bufferSize + 1);
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        return std::string(hostname.data());
    }
    return std::string("unknown");
}

int main() {
    system("clear");
    
    std::string username = getSystemUsername();
    std::string hostname = getSystemHostname();
    std::string current_dir = MagnoliaOS::getCurrentDirectory();

    std::cout << "Starting Magnolia Shell (mash) v.0.1.0...\n";
    std::cout << "Type 'exit' to shutdown.\n\n";

    std::vector<std::string> history;
    std::map<std::string, std::string> aliases;
    extern char** environ;

    auto expandAlias = [&](const std::string& token) -> std::string {
        auto it = aliases.find(token);
        return it != aliases.end() ? it->second : std::string();
    };

    auto expandCommandAliases = [&](std::vector<std::string>& argv) {
        if (argv.empty()) return;
        while (true) {
            std::string alias_value = expandAlias(argv[0]);
            if (alias_value.empty()) break;
            
            std::vector<Token> alias_tokens = parseTokens(alias_value);
            std::vector<std::string> alias_args;
            for (const auto& t : alias_tokens) {
                alias_args.push_back(t.text);
            }
            argv.erase(argv.begin());
            argv.insert(argv.begin(), alias_args.begin(), alias_args.end());
        }
    };

    std::function<bool(const std::vector<std::string>&)> executeArgv;
    
    auto executePipeline = [&](const std::string& line) -> bool {
        std::vector<std::string> segments = splitPipeline(line);
        std::vector<CommandInfo> pipeline;
        for (const auto& seg : segments) {
            CommandInfo info = parseCommandInfo(seg);
            if (!info.argv.empty()) {
                expandCommandAliases(info.argv);
                pipeline.push_back(info);
            }
        }

        if (pipeline.empty()) return true;

        // If it's a single internal command, execute in parent shell directly
        if (pipeline.size() == 1 && isInternal(pipeline[0].argv[0])) {
            int saved_stdin = -1;
            int saved_stdout = -1;

            if (!pipeline[0].stdin_file.empty()) {
                saved_stdin = dup(STDIN_FILENO);
                int fd = open(pipeline[0].stdin_file.c_str(), O_RDONLY);
                if (fd < 0) {
                    std::perror("open stdin");
                    return true;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (!pipeline[0].stdout_file.empty()) {
                saved_stdout = dup(STDOUT_FILENO);
                int flags = O_WRONLY | O_CREAT | (pipeline[0].stdout_append ? O_APPEND : O_TRUNC);
                int fd = open(pipeline[0].stdout_file.c_str(), flags, 0644);
                if (fd < 0) {
                    std::perror("open stdout");
                    if (saved_stdin >= 0) {
                        dup2(saved_stdin, STDIN_FILENO);
                        close(saved_stdin);
                    }
                    return true;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            bool ret = executeArgv(pipeline[0].argv);

            if (saved_stdin >= 0) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            if (saved_stdout >= 0) {
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }

            return ret;
        }

        // Multi-stage pipeline or single external command
        size_t num_cmds = pipeline.size();
        std::vector<pid_t> pids(num_cmds, -1);
        int prev_read_fd = -1;

        for (size_t i = 0; i < num_cmds; ++i) {
            int fd[2] = {-1, -1};
            if (i + 1 < num_cmds) {
                if (pipe(fd) < 0) {
                    std::perror("pipe");
                    break;
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                signal(SIGHUP, SIG_DFL);

                if (i > 0) {
                    dup2(prev_read_fd, STDIN_FILENO);
                    close(prev_read_fd);
                }

                if (i + 1 < num_cmds) {
                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[0]);
                    close(fd[1]);
                }

                if (!pipeline[i].stdin_file.empty()) {
                    int infile = open(pipeline[i].stdin_file.c_str(), O_RDONLY);
                    if (infile < 0) {
                        std::perror("open");
                        _exit(EXIT_FAILURE);
                    }
                    dup2(infile, STDIN_FILENO);
                    close(infile);
                }

                if (!pipeline[i].stdout_file.empty()) {
                    int flags = O_WRONLY | O_CREAT | (pipeline[i].stdout_append ? O_APPEND : O_TRUNC);
                    int outfile = open(pipeline[i].stdout_file.c_str(), flags, 0644);
                    if (outfile < 0) {
                        std::perror("open");
                        _exit(EXIT_FAILURE);
                    }
                    dup2(outfile, STDOUT_FILENO);
                    close(outfile);
                }

                if (isInternal(pipeline[i].argv[0])) {
                    executeArgv(pipeline[i].argv);
                    _exit(EXIT_SUCCESS);
                } else {
                    std::vector<char*> exec_args;
                    exec_args.reserve(pipeline[i].argv.size() + 1);
                    for (const auto& arg : pipeline[i].argv) {
                        exec_args.push_back(const_cast<char*>(arg.c_str()));
                    }
                    exec_args.push_back(nullptr);
                    execvp(exec_args[0], exec_args.data());
                    if (errno == ENOENT) {
                        std::cerr << "Command not found: " << exec_args[0] << std::endl;
                    } else {
                        std::perror("exec");
                    }
                    _exit(EXIT_FAILURE);
                }
            } else if (pid > 0) {
                pids[i] = pid;
                if (i > 0) {
                    close(prev_read_fd);
                }
                if (i + 1 < num_cmds) {
                    close(fd[1]);
                    prev_read_fd = fd[0];
                }
            } else {
                std::perror("fork");
                break;
            }
        }

        auto prev_sigint = signal(SIGINT, SIG_IGN);
        for (size_t i = 0; i < num_cmds; ++i) {
            if (pids[i] > 0) {
                int status;
                waitpid(pids[i], &status, 0);
            }
        }
        signal(SIGINT, prev_sigint);

        return true;
    };

    executeArgv = [&](const std::vector<std::string>& argv) -> bool {
        if (argv.empty()) return true;

        std::string command = argv[0];
        std::vector<std::string> args(argv.begin() + 1, argv.end());

        if (command == "exit") {
            std::cout << "Exiting...\n";
            return false;
        }
        else if (command == "echo") {
            MagnoliaOS::executeEcho(args);
        }
        else if (command == "pwd") {
            MagnoliaOS::executePwd();
        }
        else if (command == "ls") {
            MagnoliaOS::executeLs(args);
        }
        else if (command == "cd") {
            MagnoliaOS::executeCd(args);
        }
        else if (command == "mkdir") {
            MagnoliaOS::executeMkdir(args);
        }
        else if (command == "rm") {
            MagnoliaOS::executeRm(args);
        }
        else if (command == "touch") {
            MagnoliaOS::executeTouch(args);
        }
        else if (command == "cp") {
            MagnoliaOS::executeCp(args);
        }
        else if (command == "mv") {
            MagnoliaOS::executeMv(args);
        }
        else if (command == "chmod") {
            MagnoliaOS::executeChmod(args);
        }
        else if (command == "chown") {
            MagnoliaOS::executeChown(args);
        }
        else if (command == "chgrp") {
            MagnoliaOS::executeChgrp(args);
        }
        else if (command == "cat") {
            MagnoliaOS::executeCat(args);
        }
        else if (command == "history") {
            MagnoliaOS::executeHistory(history);
        }
        else if (command == "version") {
            MagnoliaOS::executeVersion();
        }
        else if (command == "whoami") {
            MagnoliaOS::executeWhoami();
        }
        else if (command == "uname") {
            MagnoliaOS::executeUname();
        }
        else if (command == "date") {
            MagnoliaOS::executeDate();
        }
        else if (command == "which") {
            MagnoliaOS::executeWhich(args);
        }
        else if (command == "kill") {
            MagnoliaOS::executeKill(args);
        }
        else if (command == "grep") {
            MagnoliaOS::executeGrep(args);
        }
        else if (command == "head") {
            MagnoliaOS::executeHead(args);
        }
        else if (command == "tail") {
            MagnoliaOS::executeTail(args);
        }
        else if (command == "find") {
            MagnoliaOS::executeFind(args);
        }
        else if (command == "free") {
            MagnoliaOS::executeFree();
        }
        else if (command == "uptime") {
            MagnoliaOS::executeUptime();
        }
        else if (command == "help") {
            MagnoliaOS::executeHelp();
        }
        else if (command == "clear") {
            MagnoliaOS::executeClear();
        }
        else if (command == "env") {
            for (char** env = environ; *env != nullptr; ++env) {
                std::cout << *env << std::endl;
            }
        }
        else if (command == "export") {
            if (args.empty()) {
                for (char** env = environ; *env != nullptr; ++env) {
                    std::cout << "export " << *env << std::endl;
                }
            } else {
                for (const auto& arg : args) {
                    size_t eq = arg.find('=');
                    if (eq == std::string::npos) {
                        std::cout << "export: invalid argument: " << arg << std::endl;
                    } else {
                        std::string name = arg.substr(0, eq);
                        std::string value = arg.substr(eq + 1);
                        setenv(name.c_str(), value.c_str(), 1);
                    }
                }
            }
        }
        else if (command == "alias") {
            if (args.empty()) {
                for (auto& [name, value] : aliases) {
                    std::cout << "alias " << name << "='" << value << "'\n";
                }
            } else {
                for (const auto& arg : args) {
                    size_t eq = arg.find('=');
                    if (eq == std::string::npos) {
                        std::cout << "alias: invalid format\n";
                    } else {
                        std::string name = arg.substr(0, eq);
                        std::string value = arg.substr(eq + 1);
                        if (!value.empty() && value.front() == '\'' && value.back() == '\'') {
                            value = value.substr(1, value.size() - 2);
                        }
                        aliases[name] = value;
                    }
                }
            }
        }
        else if (command == "unalias") {
            if (args.empty()) {
                std::cout << "unalias: missing operand\n";
            } else {
                for (const auto& arg : args) {
                    aliases.erase(arg);
                }
            }
        }
        else if (command == "source") {
            if (args.empty()) {
                std::cout << "source: filename argument required\n";
            } else {
                std::ifstream file(args[0]);
                if (!file.is_open()) {
                    std::cout << "source: cannot open '" << args[0] << "'" << std::endl;
                } else {
                    std::string source_line;
                    while (std::getline(file, source_line)) {
                        if (!source_line.empty() && source_line[0] != '#') {
                            if (!executePipeline(source_line)) return false;
                        }
                    }
                }
            }
        }

        return true;
    };

    TerminalReader reader;
    global_reader = &reader;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGHUP, signalHandler);

    while (true) {
        current_dir = MagnoliaOS::getCurrentDirectory();
        
        std::stringstream prompt_ss;
        prompt_ss << "\033[1;32m" << username << "@" << hostname << "\033[0m:"
                  << "\033[1;34m" << current_dir << "\033[0m$ ";
        std::string prompt = prompt_ss.str();

        bool should_exit = false;
        std::string input = reader.readLine(prompt, history, should_exit);

        if (should_exit) {
            break;
        }

        if (input.empty()) continue;

        if (history.empty() || history.back() != input) {
            history.push_back(input);
        }

        if (!executePipeline(input)) {
            break;
        }
    }

    return 0;
}
