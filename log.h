#ifndef LOG_H
#define LOG_H

#include <string>
#include <fstream>
#include <pthread.h>

class Log {
private:
    pthread_mutex_t mutex;
    std::ofstream logFile;
    
    Log(); // Private constructor (Singleton)

public:
    static Log& getInstance();
    ~Log();
    
    // Delete copy constructor and assignment operator
    Log(const Log&) = delete;
    void operator=(const Log&) = delete;

    void write(const std::string& message);
};

#endif