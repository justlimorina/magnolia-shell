#include<iostream>
#include<string>
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
#include "header/command.h"

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

int main(){
    system("clear");
    // Using system("cls"); if you're running Windows./.
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

    std::function<bool(const std::string&)> executeLine;
    executeLine = [&](const std::string& line) -> bool {
        if (line.empty()) return true;
        std::istringstream ss(line);
        std::string command;
        if (!(ss >> command)) return true;

        std::string rest;
        std::getline(ss, rest);
        if (!rest.empty() && rest[0] == ' ') {
            rest.erase(0, 1);
        }

        std::string alias_value = expandAlias(command);
        if (!alias_value.empty()) {
            std::string expanded = alias_value;
            if (!rest.empty()) {
                expanded += " ";
                expanded += rest;
            }
            return executeLine(expanded);
        }

        if (command == "exit") {
            std::cout << "Exiting...\n";
            return false;
        }
        else if (command == "echo") {
            MagnoliaOS::executeEcho(rest);
        }
        else if (command == "pwd") {
            MagnoliaOS::executePwd();
        }
        else if (command == "ls") {
            MagnoliaOS::executeLs(rest);
        }
        else if (command == "cd") {
            MagnoliaOS::executeCd(rest);
        }
        else if (command == "mkdir") {
            MagnoliaOS::executeMkdir(rest);
        }
        else if (command == "rm") {
            MagnoliaOS::executeRm(rest);
        }
        else if (command == "touch") {
            MagnoliaOS::executeTouch(rest);
        }
        else if (command == "cp") {
            MagnoliaOS::executeCp(rest);
        }
        else if (command == "mv") {
            MagnoliaOS::executeMv(rest);
        }
        else if (command == "chmod") {
            MagnoliaOS::executeChmod(rest);
        }
        else if (command == "chown") {
            MagnoliaOS::executeChown(rest);
        }
        else if (command == "chgrp") {
            MagnoliaOS::executeChgrp(rest);
        }
        else if (command == "cat") {
            MagnoliaOS::executeCat(rest);
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
            if (rest.empty()) {
                for (char** env = environ; *env != nullptr; ++env) {
                    std::cout << "export " << *env << std::endl;
                }
            } else {
                size_t eq = rest.find('=');
                if (eq == std::string::npos) {
                    std::cout << "export: invalid argument: " << rest << std::endl;
                } else {
                    std::string name = rest.substr(0, eq);
                    std::string value = rest.substr(eq + 1);
                    setenv(name.c_str(), value.c_str(), 1);
                }
            }
        }
        else if (command == "alias") {
            if (rest.empty()) {
                for (auto& [name, value] : aliases) {
                    std::cout << "alias " << name << "='" << value << "'\n";
                }
            } else {
                size_t eq = rest.find('=');
                if (eq == std::string::npos) {
                    std::cout << "alias: invalid format\n";
                } else {
                    std::string name = rest.substr(0, eq);
                    std::string value = rest.substr(eq + 1);
                    if (!value.empty() && value.front() == '\'' && value.back() == '\'') {
                        value = value.substr(1, value.size() - 2);
                    }
                    aliases[name] = value;
                }
            }
        }
        else if (command == "unalias") {
            if (rest.empty()) {
                std::cout << "unalias: missing operand\n";
            } else {
                aliases.erase(rest);
            }
        }
        else if (command == "source") {
            if (rest.empty()) {
                std::cout << "source: filename argument required\n";
            } else {
                std::ifstream file(rest);
                if (!file.is_open()) {
                    std::cout << "source: cannot open '" << rest << "'" << std::endl;
                } else {
                    std::string line;
                    while (std::getline(file, line)) {
                        if (!line.empty() && line[0] != '#') {
                            if (!executeLine(line)) return false;
                        }
                    }
                }
            }
        }
        else {
            std::istringstream ss_exec(line);
            std::vector<std::string> argv;
            std::string word;
            while (ss_exec >> word) {
                argv.push_back(word);
            }
            if (!argv.empty()) {
                std::vector<char*> exec_args;
                exec_args.reserve(argv.size() + 1);
                for (auto& arg : argv) {
                    exec_args.push_back(arg.data());
                }
                exec_args.push_back(nullptr);

                pid_t pid = fork();
                if (pid == 0) {
                    execvp(exec_args[0], exec_args.data());
                    if (errno == ENOENT) {
                        std::cerr << "Command not found: " << exec_args[0] << std::endl;
                    } else {
                        std::perror("exec");
                    }
                    _exit(EXIT_FAILURE);
                } else if (pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                } else {
                    std::perror("fork");
                }
            }
        }

        return true;
    };

    while (true){
        current_dir = MagnoliaOS::getCurrentDirectory();
        std::cout << "\033[1;32m" << username << "@" << hostname << "\033[0m:"
            << "\033[1;34m" << current_dir << "\033[0m$ ";

        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) continue;

        history.push_back(input);
        if (!executeLine(input)) {
            break;
        }
    }
}
