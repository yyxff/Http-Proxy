#include "Logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
using namespace std;
// singleton get instance
Logger & Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogPath(const std::string & path) {
    std::lock_guard<std::mutex> lock(mtx);
    if (isInitialized) {
        if (logFileInfo.is_open()) logFileInfo.close();
        if (logFileWarning.is_open()) logFileWarning.close();
        if (logFileDebug.is_open()) logFileDebug.close();
        if (logFileError.is_open()) logFileError.close();
    }
    
    try {
        // Create parent directory if it doesn't exist
        std::filesystem::path log_path(path);
        if (!log_path.parent_path().empty()) {
            std::filesystem::create_directories(log_path.parent_path());
        }
        
        // Open the log file
        logFileInfo.open(path+"INFO.log", std::ios::app);
        if (!logFileInfo.is_open()) {
            throw std::runtime_error("Failed to open log file: " + path);
        }

        // Open the log file
        logFileWarning.open(path+"WARNING.log", std::ios::app);
        if (!logFileInfo.is_open()) {
            throw std::runtime_error("Failed to open log file: " + path);
        }

        // Open the log file
        logFileDebug.open(path+"DEBUG.log", std::ios::app);
        if (!logFileInfo.is_open()) {
            throw std::runtime_error("Failed to open log file: " + path);
        }

        // Open the log file
        logFileError.open(path+"ERROR.log", std::ios::app);
        if (!logFileInfo.is_open()) {
            throw std::runtime_error("Failed to open log file: " + path);
        }
        isInitialized = true;
    }
    catch (const std::exception& e) {
        std::cerr << "Logger initialization error: " << e.what() << std::endl;
        // Continue without file logging, but with console output
        isInitialized = false;
    }
}

// destructor
Logger::~Logger() {
    if (logFileInfo.is_open()) logFileInfo.close();
    if (logFileWarning.is_open()) logFileWarning.close();
    if (logFileDebug.is_open()) logFileDebug.close();
}

void Logger::log(ofstream & logFile, const std::string & level, const std::string & message) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string timestamp = getCurrentTime();
    std::cout << timestamp << " [" << level << "] " << message << std::endl;
    if (isInitialized) {
        logFile << timestamp << " [" << level << "] " << message << std::endl;
        logFile.flush();
    }
}

void Logger::log(ofstream & logFile, const std::string & level, int pid, const std::string & message) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string timestamp = getCurrentTime();
    std::cout << timestamp<<" [" << level << "] " << pid<<": " << message << std::endl;
    if (isInitialized) {
        logFile << timestamp<<" [" << level << "] " << pid<<": " << message << std::endl;
        logFile.flush();
    }
}

void Logger::info(const std::string & message) {
    log(logFileInfo, "INFO", message);
    log(logFileDebug, "INFO", message);
}

void Logger::warning(const std::string & message) {
    log(logFileWarning, "WARNING", message);
    log(logFileInfo, "WARNING", message);
    log(logFileDebug, "WARNING", message);
}

void Logger::debug(const std::string & message) {
    log(logFileDebug, "DEBUG", message);
}

void Logger::info(int pid, const std::string & message) {
    log(logFileInfo, "INFO", pid, message);
    log(logFileDebug, "INFO", pid, message);
}

void Logger::warning(int pid, const std::string & message) {
    log(logFileWarning, "WARNING", pid, message);
    log(logFileInfo, "WARNING", pid, message);
    log(logFileDebug, "WARNING", pid, message);
}

void Logger::debug(int pid, const std::string & message) {
    log(logFileDebug, "DEBUG", pid, message);
}

void Logger::error(const std::string & message) {
    log(logFileError, "ERROR", message);
    log(logFileWarning, "ERROR", message);
    log(logFileInfo, "ERROR", message);
    log(logFileDebug, "ERROR", message);
}

void Logger::error(int pid, const std::string & message) {
    log(logFileError, "ERROR", pid, message);
    log(logFileWarning, "ERROR", pid, message);
    log(logFileInfo, "ERROR", pid, message);
    log(logFileDebug, "ERROR", pid, message);
}




std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
