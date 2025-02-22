#include "logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

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
    }
    
    try {
        // Create parent directory if it doesn't exist
        std::filesystem::path log_path(path);
        if (!log_path.parent_path().empty()) {
            std::filesystem::create_directories(log_path.parent_path());
        }
        
        // Open the log file
        logFileInfo.open(path, std::ios::app);
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

void Logger::log(const std::string & level, const std::string & message) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string timestamp = getCurrentTime();
    std::cout << "[" << level << "] " << timestamp << " " << message << std::endl;
    if (isInitialized) {
        logFileInfo << "[" << level << "] " << timestamp << " " << message << std::endl;
        logFileInfo.flush();
    }
}

void Logger::info(const std::string & message) {
    log("INFO", message);
}

void Logger::warning(const std::string & message) {
    log("WARNING", message);
}

void Logger::debug(const std::string & message) {
    log("DEBUG", message);
}

void Logger::error(const std::string & message) {
    log("ERROR", message);
}

void Logger::log(int id, const std::string & message) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string timestamp = getCurrentTime();
    std::cout << "[" << id << "] " << timestamp << " " << message << std::endl;
    if (isInitialized) {
        logFileInfo << "[" << id << "] " << timestamp << " " << message << std::endl;
        logFileInfo.flush();
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
