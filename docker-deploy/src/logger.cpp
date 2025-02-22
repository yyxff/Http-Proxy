#include "logger.hpp"

// singleton get instance
Logger & Logger::getInstance(){
    static Logger instance("/var/log/erss/proxy.log");
    return instance;
}


// constructor
Logger::Logger(const string & filePath){
    logFileInfo.open(filePath, std::ios::app);
    if (!logFileInfo.is_open()) {
        throw std::runtime_error("Cannot open log file");
    }
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
    logFileInfo << "(no-id): NOTE " << message << endl;
    logFileInfo.flush();
}


// log warning level message
void Logger::warning(const string & message) {
    lock_guard<mutex> lock(mtx);
    logFileInfo << "(no-id): WARNING " << message << endl;
    logFileInfo.flush();
}


// log debug level message
void Logger::debug(const string & message) {
    lock_guard<mutex> lock(mtx);
    logFileInfo << "(no-id): NOTE " << message << endl;
    logFileInfo.flush();
}


// log info level message with pid
void Logger::info(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    logFileInfo << pid << ": NOTE " << message << endl;
    logFileInfo.flush();
}


// log warning level message with pid
void Logger::warning(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    logFileInfo << pid << ": WARNING " << message << endl;
    logFileInfo.flush();
}


// log debug level message with pid
void Logger::debug(int pid, const string & message) {
    lock_guard<mutex> lock(mtx);
    logFileInfo << pid << ": NOTE " << message << endl;
    logFileInfo.flush();
}
