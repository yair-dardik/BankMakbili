#include <iostream>
#include <vector>
#include <pthread.h>
#include <cstdlib>
#include "Bank.h"
#include "ATM.h"

int main(int argc, char* argv[]) {
    

    // 1. Parse Number of VIP Threads
    int numVIPThreads = std::atoi(argv[1]);
    (void)numVIPThreads; // Suppress unused variable warning if not used here

    // 2. Determine Number of ATMs and Initialize Bank
    // The number of ATMs is the number of input files provided.
    int numATMs = argc - 2;
    
    // Initialize the Bank's internal ATM tracking structures before launching threads 
    Bank::getInstance().initWrappers(numATMs);

    // 4. Create ATM Threads [cite: 36, 218]
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

    // 5. Wait for ATMs to Finish [cite: 40]
    // The main thread waits for all ATM threads to complete their input files or be closed[cite: 41, 147].
    for (int i = 0; i < numATMs; ++i) {
        pthread_join(atmThreads[i], NULL);
    }

    // 6. Finalization
    Bank::getInstance().bankIsRunning = false; // Signal maintenance thread to stop
    
    return 0;
}