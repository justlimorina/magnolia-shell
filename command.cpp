#include "header/command.h"
#include <iostream>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <limits.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cstdio>

namespace MagnoliaOS {
    std::string getCurrentDirectory() {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            return std::string(cwd);
        }
        return std::string(".");
    }

    static std::string formatPermissions(mode_t mode) {
        std::string perms = "----------";
        perms[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
        perms[1] = (mode & S_IRUSR) ? 'r' : '-';
        perms[2] = (mode & S_IWUSR) ? 'w' : '-';
        perms[3] = (mode & S_IXUSR) ? 'x' : '-';
        perms[4] = (mode & S_IRGRP) ? 'r' : '-';
        perms[5] = (mode & S_IWGRP) ? 'w' : '-';
        perms[6] = (mode & S_IXGRP) ? 'x' : '-';
        perms[7] = (mode & S_IROTH) ? 'r' : '-';
        perms[8] = (mode & S_IWOTH) ? 'w' : '-';
        perms[9] = (mode & S_IXOTH) ? 'x' : '-';
        return perms;
    }

    static std::string formatTime(std::time_t timeValue) {
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&timeValue));
        return std::string(buffer);
    }

    static std::string formatOwnerGroup(uid_t uid, gid_t gid) {
        std::string owner = "unknown";
        std::string group = "unknown";
        if (struct passwd* pw = getpwuid(uid)) {
            owner = pw->pw_name;
        }
        if (struct group* gr = getgrgid(gid)) {
            group = gr->gr_name;
        }
        return owner + ":" + group;
    }

    void executeEcho(const std::string& args) {
        std::cout << args << std::endl;
    }

    void executeHelp() {
        std::cout << "Magnolia Shell (mash), version 0.1.0\n";
        std::cout << "These shell commands are defined internally. Type 'help' to see this list.\n\n";

        std::cout << "  echo [arg ...]     Prints the arguments to the standard output.\n";
        std::cout << "  help               Displays information about built-in commands.\n";
        std::cout << "  clear              Clears the terminal screen.\n";
        std::cout << "  pwd                Prints the current working directory.\n";
        std::cout << "  ls [path]          Lists files in the current or specified directory.\n";
        std::cout << "  cd [path]          Changes the current working directory.\n";
        std::cout << "  mkdir <path>       Creates a new directory.\n";
        std::cout << "  rm <path>          Removes a file or an empty directory.\n";
        std::cout << "  chmod <mode> <path> Changes file permissions.\n";
        std::cout << "  chown <owner[:group]> <path> Change file owner and group.\n";
        std::cout << "  chgrp <group> <path> Change file group.\n";
        std::cout << "  cat <path>         Displays a file's contents.\n";
        std::cout << "  history            Prints command history.\n";
        std::cout << "  version            Prints shell version.\n";
        std::cout << "  whoami             Prints the current user name.\n";
        std::cout << "  uname              Prints system information.\n";
        std::cout << "  date               Prints the current date and time.\n";
        std::cout << "  touch <file>       Creates an empty file or updates timestamp.\n";
        std::cout << "  cp <src> <dst>     Copies a file.\n";
        std::cout << "  mv <src> <dst>     Moves or renames a file.\n";
        std::cout << "  exit               Exits the Magnolia Shell.\n";
    }

    void executeClear() {
        std::cout << "\033[2J\033[1;1H";
    }

    void executePwd() {
        std::cout << getCurrentDirectory() << std::endl;
    }

    void executeMkdir(const std::string& args) {
        if (args.empty()) {
            std::cout << "mkdir: missing operand" << std::endl;
            return;
        }
        if (mkdir(args.c_str(), 0755) != 0) {
            std::perror("mkdir");
        }
    }

    void executeRm(const std::string& args) {
        if (args.empty()) {
            std::cout << "rm: missing operand" << std::endl;
            return;
        }
        if (remove(args.c_str()) != 0) {
            std::perror("rm");
        }
    }

    void executeCat(const std::string& args) {
        if (args.empty()) {
            std::cout << "cat: missing operand" << std::endl;
            return;
        }
        std::ifstream file(args);
        if (!file.is_open()) {
            std::cout << "cat: cannot open '" << args << "'" << std::endl;
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << std::endl;
        }
    }

    void executeTouch(const std::string& args) {
        if (args.empty()) {
            std::cout << "touch: missing file operand" << std::endl;
            return;
        }
        std::ofstream file(args, std::ios::app);
        if (!file) {
            std::cout << "touch: cannot touch '" << args << "'" << std::endl;
        }
    }

    void executeCp(const std::string& args) {
        if (args.empty()) {
            std::cout << "cp: missing file operand" << std::endl;
            return;
        }
        size_t pos = args.find(' ');
        if (pos == std::string::npos) {
            std::cout << "cp: missing destination file operand after '" << args << "'" << std::endl;
            return;
        }
        std::string source = args.substr(0, pos);
        std::string dest = args.substr(pos + 1);
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(dest, std::ios::binary);
        if (!in) {
            std::cout << "cp: cannot stat '" << source << "': No such file or directory" << std::endl;
            return;
        }
        if (!out) {
            std::cout << "cp: cannot create regular file '" << dest << "'" << std::endl;
            return;
        }
        out << in.rdbuf();
    }

    void executeMv(const std::string& args) {
        if (args.empty()) {
            std::cout << "mv: missing file operand" << std::endl;
            return;
        }
        size_t pos = args.find(' ');
        if (pos == std::string::npos) {
            std::cout << "mv: missing destination file operand after '" << args << "'" << std::endl;
            return;
        }
        std::string source = args.substr(0, pos);
        std::string dest = args.substr(pos + 1);
        if (std::rename(source.c_str(), dest.c_str()) != 0) {
            std::perror("mv");
        }
    }

    void executeChmod(const std::string& args) {
        std::istringstream ss(args);
        std::string modeStr, path;
        if (!(ss >> modeStr >> path)) {
            std::cout << "chmod: missing operand" << std::endl;
            return;
        }
        char* endptr = nullptr;
        long mode = std::strtol(modeStr.c_str(), &endptr, 8);
        if (*endptr != '\0') {
            std::cout << "chmod: invalid mode: '" << modeStr << "'" << std::endl;
            return;
        }
        if (chmod(path.c_str(), static_cast<mode_t>(mode)) != 0) {
            std::perror("chmod");
        }
    }

    void executeChown(const std::string& args) {
        std::istringstream ss(args);
        std::string ownerGroup, path;
        if (!(ss >> ownerGroup >> path)) {
            std::cout << "chown: missing operand" << std::endl;
            return;
        }
        std::string owner;
        std::string group;
        size_t colon = ownerGroup.find(':');
        if (colon == std::string::npos) {
            owner = ownerGroup;
        } else {
            owner = ownerGroup.substr(0, colon);
            group = ownerGroup.substr(colon + 1);
        }
        uid_t uid = -1;
        gid_t gid = -1;
        if (!owner.empty()) {
            if (struct passwd* pw = getpwnam(owner.c_str())) {
                uid = pw->pw_uid;
            } else {
                std::cout << "chown: invalid user: '" << owner << "'" << std::endl;
                return;
            }
        }
        if (!group.empty()) {
            if (struct group* gr = getgrnam(group.c_str())) {
                gid = gr->gr_gid;
            } else {
                std::cout << "chown: invalid group: '" << group << "'" << std::endl;
                return;
            }
        }
        if (chown(path.c_str(), uid, gid) != 0) {
            std::perror("chown");
        }
    }

    void executeChgrp(const std::string& args) {
        std::istringstream ss(args);
        std::string group, path;
        if (!(ss >> group >> path)) {
            std::cout << "chgrp: missing operand" << std::endl;
            return;
        }
        gid_t gid = -1;
        if (struct group* gr = getgrnam(group.c_str())) {
            gid = gr->gr_gid;
        } else {
            std::cout << "chgrp: invalid group: '" << group << "'" << std::endl;
            return;
        }
        if (chown(path.c_str(), -1, gid) != 0) {
            std::perror("chgrp");
        }
    }

    void executeHistory(const std::vector<std::string>& history) {
        for (size_t i = 0; i < history.size(); ++i) {
            std::cout << i + 1 << "  " << history[i] << std::endl;
        }
    }

    void executeVersion() {
        std::cout << "Magnolia Shell version 0.1.0" << std::endl;
    }

    void executeWhoami() {
        if (const char* login = getlogin()) {
            std::cout << login << std::endl;
            return;
        }
        if (struct passwd* pw = getpwuid(geteuid())) {
            std::cout << pw->pw_name << std::endl;
            return;
        }
        std::cout << "unknown" << std::endl;
    }

    void executeUname() {
        struct utsname info;
        if (uname(&info) == 0) {
            std::cout << info.sysname << " " << info.nodename << " " << info.release << " " << info.version << " " << info.machine << std::endl;
        } else {
            std::cout << "uname: failed to get system information" << std::endl;
        }
    }

    void executeDate() {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::cout << std::put_time(std::localtime(&nowTime), "%a %b %d %H:%M:%S %Z %Y") << std::endl;
    }

    void executeLs(const std::string& args) {
        std::string path = args.empty() ? "." : args;
        struct stat path_stat;
        if (stat(path.c_str(), &path_stat) != 0) {
            std::cout << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
            return;
        }

        if (!S_ISDIR(path_stat.st_mode)) {
            std::cout << path << std::endl;
            return;
        }

        DIR* dir = opendir(path.c_str());
        if (!dir) {
            std::cout << "ls: cannot open directory '" << path << "'" << std::endl;
            return;
        }

        struct dirent* entry;
        struct FileEntry { std::string name; bool hidden; bool isDir; std::string ownerGroup; std::string perms; std::string created; std::string modified; };
        std::vector<FileEntry> hiddenDirs;
        std::vector<FileEntry> normalDirs;
        std::vector<FileEntry> files;

        std::string prefix = path;
        if (!prefix.empty() && prefix.back() != '/') {
            prefix += '/';
        }

        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name == "." || name == "..") {
                continue;
            }

            bool isHidden = !name.empty() && name[0] == '.';
            std::string fullPath = prefix + name;
            struct stat entryStat;
            bool isDirectory = false;
            std::string ownerGroup = "unknown:unknown";
            std::string perms = "----------";
            std::string created = "-";
            std::string modified = "-";

            if (stat(fullPath.c_str(), &entryStat) == 0) {
                isDirectory = S_ISDIR(entryStat.st_mode);
                ownerGroup = formatOwnerGroup(entryStat.st_uid, entryStat.st_gid);
                perms = formatPermissions(entryStat.st_mode);
                created = formatTime(entryStat.st_ctime);
                modified = formatTime(entryStat.st_mtime);
            }

            FileEntry fileEntry{ name, isHidden, isDirectory, ownerGroup, perms, created, modified };
            if (isDirectory) {
                if (isHidden) hiddenDirs.push_back(fileEntry);
                else normalDirs.push_back(fileEntry);
            } else {
                files.push_back(fileEntry);
            }
        }
        closedir(dir);

        auto compareNames = [](const FileEntry& a, const FileEntry& b) {
            return a.name < b.name;
        };
        std::sort(hiddenDirs.begin(), hiddenDirs.end(), compareNames);
        std::sort(normalDirs.begin(), normalDirs.end(), compareNames);
        std::sort(files.begin(), files.end(), compareNames);

        std::vector<FileEntry> allEntries;
        allEntries.insert(allEntries.end(), hiddenDirs.begin(), hiddenDirs.end());
        allEntries.insert(allEntries.end(), normalDirs.begin(), normalDirs.end());
        allEntries.insert(allEntries.end(), files.begin(), files.end());

        size_t nameWidth = 20;
        size_t ownerWidth = 18;
        size_t permWidth = 11;
        size_t createdWidth = 17;
        size_t modifiedWidth = 17;
        for (auto& item : allEntries) {
            nameWidth = std::max(nameWidth, item.name.size() + 2);
            ownerWidth = std::max(ownerWidth, item.ownerGroup.size() + 2);
        }

        std::cout << std::left << std::setw(nameWidth) << "Name"
                  << std::setw(ownerWidth) << "Owner:Group"
                  << std::setw(permWidth) << "Permissions"
                  << std::setw(createdWidth) << "Created"
                  << std::setw(modifiedWidth) << "Modified"
                  << std::endl;
        std::cout << std::string(nameWidth + ownerWidth + permWidth + createdWidth + modifiedWidth, '-') << std::endl;

        for (auto& item : allEntries) {
            std::string color;
            if (item.isDir) {
                color = item.hidden ? "\033[35m" : "\033[36m";
            } else {
                color = "\033[0m";
            }
            std::cout << color << std::left << std::setw(nameWidth) << item.name << "\033[0m"
                      << std::setw(ownerWidth) << item.ownerGroup
                      << std::setw(permWidth) << item.perms
                      << std::setw(createdWidth) << item.created
                      << std::setw(modifiedWidth) << item.modified
                      << std::endl;
        }
    }

    void executeCd(const std::string& args) {
        const char* target = nullptr;
        std::string path;
        if (args.empty()) {
            target = getenv("HOME");
            path = target ? target : "/";
        } else {
            path = args;
            target = path.c_str();
        }

        if (chdir(target) != 0) {
            std::cout << "cd: no such file or directory: " << path << std::endl;
        }
    }
}