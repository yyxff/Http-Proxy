#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <fstream>
#include <mutex>
using namespace std;
class Logger {
public:
    static Logger & getInstance();



    // log info level message
    void info(const std::string & message);

    // log warning level message
    void warning(const std::string & message);

    // log debug level message
    void debug(const std::string & message);

    // log info level message
    void info(int pid, const std::string & message);

    // log warning level message
    void warning(int pid, const std::string & message);

    // log debug level message
    void debug(int pid, const std::string & message);

    // log error level message
    void error(const std::string & message);

    // log error level message
    void error(int pid, const std::string & message);

    // get current time
    std::string getCurrentTime();

    void setLogPath(const std::string & path);

    ~Logger();

private:
    std::ofstream logFileInfo;
    std::ofstream logFileWarning;
    std::ofstream logFileDebug;
    std::ofstream logFileError;
    std::mutex mtx;
    bool isInitialized;

    Logger() : isInitialized(false) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Generic log method for any level
    void log(ofstream & logFile, const std::string & level, const std::string & message);

    // log info level message with pid
    void log(ofstream & logFile, const std::string & level, int pid, const std::string & message);
};

#endif
