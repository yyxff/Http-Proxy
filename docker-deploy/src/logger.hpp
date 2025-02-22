#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <initializer_list>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <mutex>
using namespace std;

class Logger{

    public:
        // todo rule of three
        static Logger & getInstance();

        // log info level message
        void info(const string & message);

        // log warning level message
        void warning(const string & message);

        // log debug level message
        void debug(const string & message);

        // log info level message with pid
        void info(int pid, const string & message);

        // log warning level message with pid
        void warning(int pid, const string & message);

        // log debug level message with pid
        void debug(int pid, const string & message);

        // log to log file
        void log(int id, const string & message);

        // get current time
        string getCurrentTime();

        // destructor
        ~Logger();
    private: 
        ofstream logFileInfo;
        ofstream logFileWarning;
        ofstream logFileDebug;
        mutex mtx;

        // constructor
        Logger(const string & filePath);

        // print log to log file
        void log(ofstream & logFile, const string & message);
};

#endif
