#include "Logger.hpp"

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

Logger::~Logger(){}

void Logger::log(ofstream & logFile, const string & message){
    string timeStr = getCurrentTime();
    if (logFile.is_open()){
        logFile<<timeStr<<" "<<message<<endl;
    }
}

string Logger::getCurrentTime() {
    auto now = time(nullptr);
    auto tm = *localtime(&now);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::info(const string & message){
    string messageInfo = "[INFO]: "+message;
    log(logFileInfo, messageInfo);
    log(logFileDebug, messageInfo);
}

void Logger::warning(const string & message){
    string messageWarning = "[WARNING]: "+message;
    log(logFileWarning, messageWarning);
    log(logFileInfo, messageWarning);
    log(logFileDebug, messageWarning);
}

void Logger::debug(const string & message){
    string messageDebug = "[DEBUG]: "+message;
    log(logFileDebug, messageDebug);
}