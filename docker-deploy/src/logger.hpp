#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static Logger & getInstance();

    // Generic log method for any level
    void log(const std::string & level, const std::string & message);

    // log info level message
    void info(const std::string & message);

    // log warning level message
    void warning(const std::string & message);

    // log debug level message
    void debug(const std::string & message);

    // log error level message
    void error(const std::string & message);

    // log info level message with id
    void log(int id, const std::string & message);

    // get current time
    std::string getCurrentTime();

    void setLogPath(const std::string & path);

    ~Logger();

private:
    std::ofstream logFileInfo;
    std::ofstream logFileWarning;
    std::ofstream logFileDebug;
    std::mutex mtx;
    bool isInitialized;

    Logger() : isInitialized(false) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

#endif
