#include "VIPManager.h"

VIPManager::VIPManager() : isFinished(false) {
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&cv_empty, NULL);
}

VIPManager::~VIPManager() {
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv_empty);
}

VIPManager& VIPManager::getInstance() {
    static VIPManager instance;
    return instance;
}

void VIPManager::addVIPCommand(int priority, int atmID, std::string cmd) {
    pthread_mutex_lock(&m);
    
    VIPCommand task;
    task.priority = priority;
    task.atmID = atmID;
    task.line = cmd;
    
    tasks.push(task);
    
    // Signal a waiting VIP thread that work is available 
    pthread_cond_signal(&cv_empty);
    pthread_mutex_unlock(&m);
}

// Consumer: VIP Worker threads call this 
VIPCommand VIPManager::getVIPCommand() {
    pthread_mutex_lock(&m);
    
    // Wait while the queue is empty AND the bank is still running
    while (tasks.empty() && !isFinished) {
        pthread_cond_wait(&cv_empty, &m);
    }
    
    // If shutting down and no tasks remain, return "poison pill"
    if (isFinished && tasks.empty()) {
        pthread_mutex_unlock(&m);
        return {-1, -1, ""}; 
    }
    
    // Pop highest priority task
    VIPCommand task = tasks.top();
    tasks.pop();
    
    pthread_mutex_unlock(&m);
    return task;
}

void VIPManager::quit() {
    pthread_mutex_lock(&m);
    isFinished = true;
    pthread_cond_broadcast(&cv_empty); // Wake all VIP threads to exit
    pthread_mutex_unlock(&m);
}