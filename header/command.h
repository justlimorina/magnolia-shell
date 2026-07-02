#pragma once
#include <string>
#include <vector>

namespace MagnoliaOS {
    void executeEcho(const std::vector<std::string>& args);
    void executeHelp();
    void executeClear();
    void executePwd();
    void executeLs(const std::vector<std::string>& args);
    void executeCd(const std::vector<std::string>& args);
    void executeMkdir(const std::vector<std::string>& args);
    void executeRm(const std::vector<std::string>& args);
    void executeTouch(const std::vector<std::string>& args);
    void executeCp(const std::vector<std::string>& args);
    void executeMv(const std::vector<std::string>& args);
    void executeChmod(const std::vector<std::string>& args);
    void executeChown(const std::vector<std::string>& args);
    void executeChgrp(const std::vector<std::string>& args);
    void executeCat(const std::vector<std::string>& args);
    void executeHistory(const std::vector<std::string>& history);
    void executeVersion();
    void executeWhoami();
    void executeUname();
    void executeDate();
    void executeWhich(const std::vector<std::string>& args);
    void executeKill(const std::vector<std::string>& args);
    void executeGrep(const std::vector<std::string>& args);
    void executeHead(const std::vector<std::string>& args);
    void executeTail(const std::vector<std::string>& args);
    void executeFind(const std::vector<std::string>& args);
    void executeFree();
    void executeUptime();
    std::string getCurrentDirectory();
}