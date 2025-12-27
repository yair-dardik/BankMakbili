#include "RWLock.h"

RWLock::RWLock() : readers(0), waitingWriters(0), activeWriter(false) {
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&cv_write, NULL);
    pthread_cond_init(&cv_read, NULL);
}

RWLock::~RWLock() {
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv_write);
    pthread_cond_destroy(&cv_read);
}

void RWLock::readLock() {
    pthread_mutex_lock(&m);
    while (activeWriter || waitingWriters > 0) {
        pthread_cond_wait(&cv_read, &m);
    }
    readers++;
    pthread_mutex_unlock(&m);
}

void RWLock::readUnlock() {
    pthread_mutex_lock(&m);
    readers--;
    if (readers == 0) {
        pthread_cond_signal(&cv_write);
    }
    pthread_mutex_unlock(&m);
}

void RWLock::writeLock() {
    pthread_mutex_lock(&m);
    waitingWriters++;
    while (readers > 0 || activeWriter) {
        pthread_cond_wait(&cv_write, &m);
    }
    waitingWriters--;
    activeWriter = true;
    pthread_mutex_unlock(&m);
}

void RWLock::writeUnlock() {
    pthread_mutex_lock(&m);
    activeWriter = false;
    if (waitingWriters > 0) {
        pthread_cond_signal(&cv_write);
    } else {
        pthread_cond_broadcast(&cv_read);
    }
    pthread_mutex_unlock(&m);
}