#ifndef VIPMANAGER_H
#define VIPMANAGER_H

#include <queue>
#include <string>
#include <pthread.h>

struct VIPCommand {
    int priority;
    int atmID;
    std::string line; // The command without "VIP=X"

    // Max-Heap: Higher priority comes first
    bool operator<(const VIPCommand& other) const {
        return priority < other.priority;
    }
};

class VIPManager {
private:
    std::priority_queue<VIPCommand> tasks;
    
    // Synchronization primitives
    pthread_mutex_t m;
    pthread_cond_t cv_empty; // Wait here if queue is empty (Consumers)

    bool isFinished;   // Flag to signal shutdown

    VIPManager();

public:
    static VIPManager& getInstance();
    ~VIPManager();

    // Producer: Called by ATM parsing logic
    void addVIPCommand(int priority, int atmID, std::string cmd);

    // Consumer: Called by VIP Worker Threads
    // Returns a command with atmID = -1 if the system is shutting down
    VIPCommand getVIPCommand();

    // Signals all threads to stop waiting and exit
    void quit();
};

#endif