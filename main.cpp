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
#include "header/command.h"

// Forward declaration of TerminalReader for signal handler
class TerminalReader;
static TerminalReader* global_reader = nullptr;

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
        // Disable echo, canonical mode, extended input processing, and signals (Ctrl+C, Ctrl+Z)
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        // Disable flow control, parity, carriage return translation
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
        std::string temp_input; // stores current input when navigating history
        size_t cursor_pos = 0;
        size_t history_index = history.size();

        auto redraw = [&]() {
            std::cout << "\r" << prompt << current_input << "\033[K";
            std::cout << "\r" << prompt << current_input.substr(0, cursor_pos) << std::flush;
        };

        // Initial draw
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
            } else if (c == 127 || c == 8) { // Backspace or Ctrl+H
                if (cursor_pos > 0) {
                    current_input.erase(cursor_pos - 1, 1);
                    cursor_pos--;
                    redraw();
                }
            } else if (c == 3) { // Ctrl+C
                std::cout << "^C\r\n";
                disableRawMode();
                return "";
            } else if (c == 4) { // Ctrl+D
                if (current_input.empty()) {
                    std::cout << "exit\r\n";
                    should_exit = true;
                    disableRawMode();
                    return "exit";
                }
            } else if (c == '\033') { // Escape sequence
                // Setup short timeout to read escape seq body
                struct termios raw;
                tcgetattr(STDIN_FILENO, &raw);
                struct termios temp_raw = raw;
                temp_raw.c_cc[VMIN] = 0;
                temp_raw.c_cc[VTIME] = 1; // 100ms timeout
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
                                if (seq[1] == '3') { // Delete key
                                    if (cursor_pos < current_input.size()) {
                                        current_input.erase(cursor_pos, 1);
                                        redraw();
                                    }
                                }
                            }
                        } else {
                            switch (seq[1]) {
                                case 'A': // Up Arrow
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
                                case 'B': // Down Arrow
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
                                case 'C': // Right Arrow
                                    if (cursor_pos < current_input.size()) {
                                        cursor_pos++;
                                        redraw();
                                    }
                                    break;
                                case 'D': // Left Arrow
                                    if (cursor_pos > 0) {
                                        cursor_pos--;
                                        redraw();
                                    }
                                    break;
                                case 'H': // Home
                                    cursor_pos = 0;
                                    redraw();
                                    break;
                                case 'F': // End
                                    cursor_pos = current_input.size();
                                    redraw();
                                    break;
                            }
                        }
                    } else if (seq[0] == 'O') {
                        switch (seq[1]) {
                            case 'H': // Home
                                cursor_pos = 0;
                                redraw();
                                break;
                            case 'F': // End
                                cursor_pos = current_input.size();
                                redraw();
                                break;
                        }
                    }
                }
            } else if (c >= 32 && c <= 126) { // Printable characters
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

static std::vector<std::string> parseCommandLine(const std::string& line) {
    std::vector<std::string> argv;
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
            }
        } else if (c == '\'') {
            if (in_double_quotes) {
                current += c;
            } else {
                in_single_quotes = !in_single_quotes;
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
                if (!current.empty()) {
                    argv.push_back(expandTilde(current));
                    current.clear();
                }
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        argv.push_back(expandTilde(current));
    }
    return argv;
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

    std::function<bool(const std::vector<std::string>&)> executeArgv;
    executeArgv = [&](const std::vector<std::string>& argv) -> bool {
        if (argv.empty()) return true;

        std::string command = argv[0];
        std::vector<std::string> args(argv.begin() + 1, argv.end());

        std::string alias_value = expandAlias(command);
        if (!alias_value.empty()) {
            std::vector<std::string> expanded_argv = parseCommandLine(alias_value);
            expanded_argv.insert(expanded_argv.end(), args.begin(), args.end());
            return executeArgv(expanded_argv);
        }

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
                            std::vector<std::string> sub_argv = parseCommandLine(source_line);
                            if (!executeArgv(sub_argv)) return false;
                        }
                    }
                }
            }
        }
        else {
            std::vector<char*> exec_args;
            exec_args.reserve(argv.size() + 1);
            for (const auto& arg : argv) {
                exec_args.push_back(const_cast<char*>(arg.c_str()));
            }
            exec_args.push_back(nullptr);

            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                signal(SIGHUP, SIG_DFL);

                execvp(exec_args[0], exec_args.data());
                if (errno == ENOENT) {
                    std::cerr << "Command not found: " << exec_args[0] << std::endl;
                } else {
                    std::perror("exec");
                }
                _exit(EXIT_FAILURE);
            } else if (pid > 0) {
                auto prev_sigint = signal(SIGINT, SIG_IGN);
                int status;
                waitpid(pid, &status, 0);
                signal(SIGINT, prev_sigint);
            } else {
                std::perror("fork");
            }
        }

        return true;
    };

    TerminalReader reader;
    global_reader = &reader;
    
    // Register signal handlers to restore cooked terminal mode on abnormal termination
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

        std::vector<std::string> parsed_argv = parseCommandLine(input);
        if (!executeArgv(parsed_argv)) {
            break;
        }
    }

    return 0;
}
