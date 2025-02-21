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
#include <fcntl.h>
#include <initializer_list>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
using namespace std;

// todo mutex for logger
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
    private: 
        ofstream logFileInfo;
        ofstream logFileWarning;
        ofstream logFileDebug;

        // constructor
        Logger(const string & filePath);

        // destructor
        ~Logger();

        // print log to log file
        void log(ofstream & logFile, const string & message);

        // get current time
        string getCurrentTime();
};

#endif