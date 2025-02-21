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
        Logger(const string & filePath);
        ~Logger();

        void info(const string & message);
        void warning(const string & message);
        void debug(const string & message);

    private: 
        ofstream logFileInfo;
        ofstream logFileWarning;
        ofstream logFileDebug;


        void log(ofstream & logFile, const string & message);
        string getCurrentTime();
};