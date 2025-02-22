#include "logger.hpp"
#include <filesystem>

// singleton get instance
Logger & Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogPath(const string & path) {
    lock_guard<mutex> lock(mtx);
    if (logFileInfo.is_open()) {
        logFileInfo.close();
    }
    
    // Create directories if they don't exist
    filesystem::path logPath(path);
    filesystem::create_directories(logPath.parent_path());
    
    logFileInfo.open(path, std::ios::app);
    if (!logFileInfo.is_open()) {
        throw std::runtime_error("Cannot open log file: " + path);
    }
    isInitialized = true;
}

// destructor
Logger::~Logger() {
    if (logFileInfo.is_open()) {
        logFileInfo.close();
    }
}

// print log to log file
void Logger::log(int id, const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << id << ": " << message << endl;
    logFileInfo.flush();
}

// get current time
string Logger::getCurrentTime() {
    time_t now = time(0);
    return string(asctime(gmtime(&now)));
}

// log info level message
void Logger::info(const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << "(no-id): NOTE " << message << endl;
    logFileInfo.flush();
}

// log warning level message
void Logger::warning(const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << "(no-id): WARNING " << message << endl;
    logFileInfo.flush();
}

// log debug level message
void Logger::debug(const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << "(no-id): NOTE " << message << endl;
    logFileInfo.flush();
}

// log info level message with pid
void Logger::info(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << pid << ": NOTE " << message << endl;
    logFileInfo.flush();
}

// log warning level message with pid
void Logger::warning(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << pid << ": WARNING " << message << endl;
    logFileInfo.flush();
}

// log debug level message with pid
void Logger::debug(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    if (!isInitialized) {
        throw std::runtime_error("Logger not initialized. Call setLogPath first.");
    }
    logFileInfo << pid << ": NOTE " << message << endl;
    logFileInfo.flush();
}
