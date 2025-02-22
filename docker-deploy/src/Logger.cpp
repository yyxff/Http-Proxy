#include "Logger.hpp"


// singleton get instance
Logger & Logger::getInstance(){
    static Logger instance("./../logs");
    static mutex mtx;
    return instance;
}


// constructor
Logger::Logger(const string & filePath){
        // create directory if doesn't exist
        if (!filesystem::exists(filePath)){
            try{
                if (filesystem::create_directories(filePath)){
                    cout<<"created log directory!"<<endl;
                }
            }catch(const filesystem::filesystem_error & e){
                cerr<<"failed to create log directory: "<<e.what()<<endl;
                exit(1);
            }
        }

        logFileInfo.open(filePath + "/INFO.log", ios::app);
        if (!logFileInfo) {
            cerr << "Failed to create INFO.log!" << endl;
            exit(1);
        }

        logFileWarning.open(filePath + "/WARNING.log", ios::app);
        if (!logFileWarning) {
            cerr << "Failed to create WARNING.log!" << endl;
            exit(1);
        }

        logFileDebug.open(filePath + "/DEBUG.log", ios::app);
        if (!logFileDebug) {
            cerr << "Failed to create DEBUG.log!" << endl;
            exit(1);
        } 
    }


// destructor
Logger::~Logger(){}


// print log to log file
void Logger::log(ofstream & logFile, const string & message){
    lock_guard<mutex> lock(mtx);
    string timeStr = getCurrentTime();
    if (logFile.is_open()){
        logFile<<timeStr<<" "<<message<<endl;
    }
}


// get current time
string Logger::getCurrentTime() {
    auto now = time(nullptr);
    auto tm = *localtime(&now);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}


// log info level message
void Logger::info(const string & message){
    string messageInfo = "[INFO]: "+message;
    log(logFileInfo, messageInfo);
    log(logFileDebug, messageInfo);
}


// log warning level message
void Logger::warning(const string & message){
    string messageWarning = "[WARNING]: "+message;
    log(logFileWarning, messageWarning);
    log(logFileInfo, messageWarning);
    log(logFileDebug, messageWarning);
}


// log debug level message
void Logger::debug(const string & message){
    string messageDebug = "[DEBUG]: "+message;
    log(logFileDebug, messageDebug);
}


// log info level message with pid
void Logger::info(int pid, const string & message){
    string messageInfo = to_string(pid)+": "+message;
    info(messageInfo);
}


// log warning level message with pid
void Logger::warning(int pid, const string & message){
    string messageWarning = to_string(pid)+": "+message;
    info(messageWarning);
}


// log debug level message with pid
void Logger::debug(int pid, const string & message){
    string messageDebug = to_string(pid)+": "+message;
    info(messageDebug);
}