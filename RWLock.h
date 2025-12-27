#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

class RWLock {
private:
    int readers;
    int waitingWriters;
    bool activeWriter;

    pthread_mutex_t m;
    pthread_cond_t cv_write; 
    pthread_cond_t cv_read; 

public:
    RWLock();
    ~RWLock();
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();
};

#endif