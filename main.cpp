#include <iostream>
#include <vector>
#include <pthread.h>
#include <cstdlib>
#include "Bank.h"
#include <cstring> // For std::strlen
#include "VIPManager.h"
#include "log.h"
#include "ATM.h"

// The routine that VIP threads will run
void* vip_routine(void* arg) {
    while (true) {
        // 1. Fetch next high-priority command
        // This blocks if the queue is empty
        VIPCommand cmd = VIPManager::getInstance().getVIPCommand();

        // 2. Check for "Poison Pill" (Shutdown signal)
        if (cmd.atmID == -1) {
            break;
        }

        // 3. Execute the command immediately (No delays)
        // We reuse the static parsing logic from the ATM class
        ATM::parseAndExecute(cmd.atmID, cmd.line);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    
    // 0. Basic Argument Check
    if (argc < 3) {
        std::cerr << "Bank error: illegal arguments" << std::endl;
        return 1;
    }
    // check if argv[1] is a positive integer
    for (size_t i = 0; i < std::strlen(argv[1]); ++i) {
        if (!std::isdigit(argv[1][i])) {
            std::cerr << "Bank error: illegal arguments" << std::endl;
            return 1;
        }
    }
    // 1. Parse Number of VIP Threads
    int numVIPThreads = std::atoi(argv[1]);

    // 2. Determine Number of ATMs and Initialize Bank
    // The number of ATMs is the number of input files provided.
    int numATMs = argc - 2;
    
    // Initialize the Bank's internal ATM tracking structures before launching threads 
    Bank::getInstance().initWrappers(numATMs);
    // Create these objects now, sequentially, so their mutexes 
    // are fully built before any thread tries to use them.
    VIPManager::getInstance(); 
    Log::getInstance();
    // 4. Create VIP Threads
    // These start running immediately but will wait (sleep) 
    // inside getVIPCommand() until tasks are added.
    std::vector<pthread_t> vipThreads(numVIPThreads);
    for (int i = 0; i < numVIPThreads; ++i) {
        if (pthread_create(&vipThreads[i], NULL, vip_routine, NULL) != 0) {
            perror("Bank error: pthread_create failed");
            return 1;
        }
    }

    // 5. Create ATM Threads 
    std::vector<pthread_t> atmThreads(numATMs);
    for (int i = 0; i < numATMs; ++i) {
        // Prepare arguments for the ATM thread
        ATMArgs* args = new ATMArgs();
        args->id = i + 1; // ATM IDs start from 1
        args->inputFile = argv[i + 2]; // File paths start from argv[2] 

        // Create the thread using the static routine in the ATM class
        if (pthread_create(&atmThreads[i], NULL, ATM::atmRoutine, (void*)args) != 0) {
            perror("Bank error: pthread_create failed"); // 
            // Clean up allocated memory if creation fails
            delete args;
            return 1;
        }
    }

    // 6. Wait for ATMs to Finish 
    // The main thread waits for all ATM threads to complete their input files or be closed.
    for (int i = 0; i < numATMs; ++i) {
        pthread_join(atmThreads[i], NULL);
    }

    // 7. Shutdown VIP Threads
    // Now that all ATMs are finished, no new VIP commands will be generated.
    // We signal the VIP manager to wake up all VIP threads and tell them to exit.
    VIPManager::getInstance().quit();

    for (int i = 0; i < numVIPThreads; ++i) {
        pthread_join(vipThreads[i], NULL);
    }

    // 8. Finalization
    Bank::getInstance().bankShutdown(); // Signal maintenance thread to stop
    
    return 0;
}