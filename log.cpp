#include "log.h"
#include <iostream>

Log::Log() {
    pthread_mutex_init(&mutex, NULL);
    logFile.open("log.txt", std::ios::out | std::ios::trunc); // Overwrite mode
}

Log::~Log() {
    if (logFile.is_open()) {
        logFile.close();
    }
    pthread_mutex_destroy(&mutex);
}

Log& Log::getInstance() {
    static Log instance;
    return instance;
}

void Log::write(const std::string& message) {
    pthread_mutex_lock(&mutex);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
    pthread_mutex_unlock(&mutex);
}