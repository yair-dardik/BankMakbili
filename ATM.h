#ifndef ATM_H
#define ATM_H

#include <string>
#include <vector>

// Struct to pass arguments from main() to the ATM thread
struct ATMArgs {
    int id;
    const char* inputFile;
};

class ATM {
public:
    // The main entry point for the thread
    // Valid signature for pthread_create: void* func(void*)
    static void* atmRoutine(void* arg);
    // Helper to parse a single line and call the appropriate Bank method
    // Returns false if the ATM should shut down (e.g. after a self-close command)
    static void parseAndExecute(int atmID, const std::string& line);
};

#endif