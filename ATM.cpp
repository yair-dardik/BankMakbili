#include "ATM.h"
#include "Bank.h"
#include "log.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h> // for usleep

// Private Helper to parse line and call Bank
void ATM::parseAndExecute(int atmID, const std::string& line) {
    Bank& bank = Bank::getInstance();
    std::stringstream ss(line);
    char cmd;
    ss >> cmd;

    // Variables to hold parsed arguments
    int accountID, password, amount, targetID, time;
    bool isDollar;
    std::string currencystr;
    int ils, usd;
    std::string dummy; // To catch "to" or other filler words
    
    // Note: The Bank methods sleep(1) internally now, 
    // so we don't need to sleep here for operations.
    
    switch (cmd) {
        case 'O': // Open Account
            ss >> accountID >> password >> ils >> usd;
            bank.createAccount(accountID, password, ils, usd, atmID);
            break;
            
        case 'D': // Deposit
            ss >> accountID >> password >> amount >> currencystr;
            isDollar = (currencystr == "USD");
            bank.deposit(accountID, password, amount, isDollar, false, atmID); 
            break;
            
        case 'W': // Withdraw
            ss >> accountID >> password >> amount >> currencystr;
            isDollar = (currencystr == "USD");
            bank.withdraw(accountID, password, amount, isDollar, false, atmID);
            break;
            
        case 'B': // Balance
            ss >> accountID >> password;
            bank.getBalance(accountID, password, atmID);
            break;
            
        case 'T': // Transfer
            ss >> accountID >> password >> targetID >> amount >> currencystr;
            isDollar = (currencystr == "USD");
            bank.transfer(accountID, password, targetID, amount, isDollar, atmID);
            break;
            
        case 'C': // Close ATM Command
            int targetAtmID;
            ss >> targetAtmID;
            bank.closeAtm(targetAtmID, atmID);
            break;
            
        case 'Q': // Close Account
            ss >> accountID >> password;
            bank.closeAccount(accountID, password, atmID);
            break;
        
        case 'R': // Rollback
            int steps;
            ss >> steps;
            bank.rollback(steps, atmID);
            break;

        case 'X': // Exchange
            bool isILStoDollar;
            ss >> accountID >> password >> currencystr >> dummy >> dummy >> amount;
            isILStoDollar = (currencystr == "ILS");
            bank.exchange(accountID, password, amount, isILStoDollar, atmID);
            break;
        
        case 'I': // Invest
            ss >> accountID >> password >> amount >> currencystr >> time;
            isDollar = (currencystr == "USD");
            bank.invest(accountID, password, amount, isDollar, time, atmID);
            break;

        case 'S': // Sleep
            ss >> time;
            Log::getInstance().write(std::to_string(atmID) 
                + ": Currently on a scheduled break. Service will resume within " +
                                     std::to_string(time) + " ms");
            usleep(time * 1000); // time is in milliseconds
            break;
        default:
            printf(("ATM " + std::to_string(atmID) + ": Unknown command '" + cmd + "'\n").c_str());
            break;
    }
}

void* ATM::atmRoutine(void* arg) {
    // 1. Recover Arguments
    ATMArgs* args = (ATMArgs*)arg;
    int myID = args->id;
    std::string filePath = args->inputFile;
    
    // We can delete args here if we copied everything we need
    delete args; 

    // 2. Open File
    std::ifstream infile(filePath);
    if (!infile.good()) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return NULL;
    }

    std::string line;
    while (std::getline(infile, line)) {
    
        // 3. Check for Shutdown Signal
        int closerID = Bank::getInstance().isAtmClosing(myID);
        if (closerID > 0) {
            // Log the death
            Log::getInstance().write("Bank: ATM " + std::to_string(closerID) + 
                                     " closed " + std::to_string(myID) + " successfully");
            break; // Exit loop
        }

        // 4. Execute Command
        parseAndExecute(myID, line);
        
    }

    // 5. Cleanup
    infile.close();
    return NULL;
}