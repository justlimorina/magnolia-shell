#pragma once
#include <string>
#include <vector>

namespace MagnoliaOS {
    void executeEcho(const std::string& args);
    void executeHelp();
    void executeClear();
    void executePwd();
    void executeLs(const std::string& args);
    void executeCd(const std::string& args);
    void executeMkdir(const std::string& args);
    void executeRm(const std::string& args);
    void executeTouch(const std::string& args);
    void executeCp(const std::string& args);
    void executeMv(const std::string& args);
    void executeChmod(const std::string& args);
    void executeChown(const std::string& args);
    void executeChgrp(const std::string& args);
    void executeCat(const std::string& args);
    void executeHistory(const std::vector<std::string>& history);
    void executeVersion();
    void executeWhoami();
    void executeUname();
    void executeDate();
    std::string getCurrentDirectory();
}
#pragma once