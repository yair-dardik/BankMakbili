#include "Bank.h"
#include "Log.h"
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <cstdio> // for printf

// Helper wrapper for the thread
void* maintenance_wrapper(void* arg) {
    Bank::getInstance().runMaintenance();
    return NULL;
}

void Bank::initWrappers(int numATMs) {
    atmLock.writeLock();
    // Initialize logic: 0 = Running
    atmStatus.assign(numATMs, 0); 
    atmLock.writeUnlock();
}

Bank::Bank() : bankIsRunning(true) {
    pthread_mutex_init(&historyLock, NULL);
    // Start the maintenance thread immediately
    pthread_create(&maintenanceThread, NULL, maintenance_wrapper, NULL);
}

Bank& Bank::getInstance() {
    static Bank instance;
    return instance;
}

// Helper function: Caller MUST hold bankLock (Read or Write)
Account* Bank::getAccount(int id, int atmID) {
    std::map<int, Account*>::iterator it = accounts.find(id);
    
    if (it == accounts.end()) {
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - account id " + std::to_string(id) + " does not exist");
        return NULL;
    }
    
    return it->second;
}

// ------------------------------------------------------------
// MAINTENANCE LOOP
// ------------------------------------------------------------
void Bank::runMaintenance() {
    int counter = 0;
    while(bankIsRunning) {
        // Sleep for 10 milliseconds (10,000 microseconds)
        // This sets the base rhythm for Printing and Snapshots [cite: 240]
        usleep(10000); 
        
        // --- Check Requests ---
        atmLock.writeLock(); 
        for (size_t i = 0; i < atmStatus.size(); ++i) {
            // Check for NEGATIVE value (Request)
            if (atmStatus[i] < 0) {
                // Flip to POSITIVE (Execute Shutdown)
                atmStatus[i] = std::abs(atmStatus[i]);
                // The ATM will log it when it wakes up and sees the positive value.
            }
        }
        atmLock.writeUnlock();
        // ----------------------

        // Lock Bank for Reading during snapshoot and printing
        bankLock.readLock();
        //lock all accounts
        for(auto const& item : accounts) {
        Account* liveAcc = item.second;
        liveAcc->lock->readLock();
        }
        // 1. Snapshot (Every 10ms)
        takeSnapshot();
        
        // 2. Print Status (Every 10ms)
        printStatus();

        //unlock the accounts
        for(auto const& item : accounts) {
            Account* liveAcc = item.second;
            liveAcc->lock->readUnlock();
        }
         bankLock.readUnlock();

        // 3. Commission (Every 30ms)
        // Since the loop runs every 10ms, we collect commission every 3rd loop.
        counter++;
        if (counter % 3 == 0) {
            collectCommission();
        }
    }
}


// ------------------------------------------------------------
// SNAPSHOT PRINT & COMISSION
// ------------------------------------------------------------

void Bank::takeSnapshot() {
    // We need to verify what locking is needed.
    // Ideally, we lock the Bank for READ to iterate the map.
    // However, to get a consistent "Point in Time", we arguably should lock Bank for WRITE 
    // to stop all transactions while copying. 
    // Given "Maximize Parallelism", Read Lock is better, but accounts might change MID-COPY.
    // For this exercise, locking Bank Read + Individual Account Read is the safe, parallel way.
    
    std::map<int, Account> currentSnap;
    
    
    for(auto const& item : accounts) {
        Account* liveAcc = item.second;
        currentSnap.insert(std::make_pair(item.first, *liveAcc)); // Deep copy using Copy Constructor    
        }

    pthread_mutex_lock(&historyLock);
    history.push_front(currentSnap);
    if (history.size() > 100) { 
        history.pop_back();
    }
    pthread_mutex_unlock(&historyLock);
}

// ------------------------------------------------------------
// ATM COMMANDS
// ------------------------------------------------------------

void Bank::createAccount(int id, int pass, int ils, int usd, int atmID) {
    // 1. Lock Bank (Write) - We are changing the map structure
    bankLock.writeLock(); 

    if (accounts.find(id) != accounts.end()) {
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - account with the same id exists");
        bankLock.writeUnlock();
        return;
    }

    // 2. Create and Insert
    Account* newAcc = new Account(id, pass, ils, usd);
    accounts.insert(std::pair<int, Account*>(id, newAcc));
    
    // 3. Log
    Log::getInstance().write(std::to_string(atmID) + ": New account id is " + std::to_string(id) + 
        " with password " + std::to_string(pass) + 
        " and initial balance " + std::to_string(ils) + " ILS and " + std::to_string(usd) + " USD");

    bankLock.writeUnlock();
}

void Bank::closeAccount(int id, int pass, int atmID) {
    // 1. Lock Bank (Write) - We are changing the map structure
    bankLock.writeLock(); 
    
    Account* acc = getAccount(id, atmID); 
    if (acc == NULL) { 
        bankLock.writeUnlock();
        return; 
    }

    // 2. Lock Account (Write) - We are reading password and balance
    acc->lock->writeLock();
    
    if (!acc->checkPassword(pass)) {
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - password for account id " + std::to_string(id) + " is incorrect");
        acc->lock->writeUnlock();
        bankLock.writeUnlock();
        return;
    }


    // 3. Remove and Delete
    accounts.erase(id);
    acc->lock->writeUnlock(); // Unlock before deletion
    delete acc;

    Log::getInstance().write(std::to_string(atmID) + ": Account id " + std::to_string(id) + " is closed");

    bankLock.writeUnlock();
}

void Bank::withdraw(int id, int pass, int amount, bool isDollar, int atmID) {
    // 1. Lock Bank (Read) - Find the account
    bankLock.readLock();
    
    Account* acc = getAccount(id, atmID); 
    if (acc == NULL) { 
        bankLock.readUnlock();
        return; 
    }

    // 2. Lock Account (Write) - Changing balance
    acc->lock->writeLock();
    
    // Check Password
    if (!acc->checkPassword(pass)) {
         Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - password for account id " + std::to_string(id) + " is incorrect");
         acc->lock->writeUnlock();
         bankLock.readUnlock();
         return;
    }

    // Check Balance based on Currency
    bool hasEnough = isDollar ? (acc->balanceUSD >= amount) : (acc->balanceILS >= amount);

    if (!hasEnough) {
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - account id " + std::to_string(id) + " balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
        std::to_string(acc->balanceUSD) + " USD is lower than " + std::to_string(amount) + (isDollar ? " USD" : " ILS"));
        acc->lock->writeUnlock();
        bankLock.readUnlock();
        return;
    }

    // Execute Withdrawal
    
    if (isDollar) {
        acc->balanceUSD -= amount;
    } else {
        acc->balanceILS -= amount;
    }
        
    Log::getInstance().write(std::to_string(atmID) + ": Account " + std::to_string(id) + 
        " new balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
        std::to_string(acc->balanceUSD) + " USD after " + std::to_string(amount) + (isDollar ? " USD" : " ILS") + " was withdrawn");
        
    acc->lock->writeUnlock();
    bankLock.readUnlock();
}


void Bank::deposit(int id, int pass, int amount, bool isDollar, int atmID) {
    // 1. Lock Bank (Read) - Find the account
    bankLock.readLock();
    
    Account* acc = getAccount(id, atmID); 
    if (acc == NULL) { 
        bankLock.readUnlock();
        return; 
    }
    
    // 2. Lock Account (Write) - Changing balance
    acc->lock->writeLock();
    
    if (!acc->checkPassword(pass)) {
         Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - password for account id " + std::to_string(id) + " is incorrect");
         acc->lock->writeUnlock();
         bankLock.readUnlock();
         return;
    }

    // Execute Deposit
    std::string currType = isDollar ? " USD" : " ILS";

    if (isDollar) {
        acc->balanceUSD += amount;
    } else {
        acc->balanceILS += amount;
    }
    
    Log::getInstance().write(std::to_string(atmID) + ": Account " + std::to_string(id) + 
        " new balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
        std::to_string(acc->balanceUSD) + " USD after " + std::to_string(amount) + currType + " was deposited");
        
    acc->lock->writeUnlock();
    bankLock.readUnlock();
}

void Bank::getBalance(int id, int pass, int atmID) {
    // 1. Lock Bank (Read) - Find the account
    bankLock.readLock();
    
    Account* acc = getAccount(id, atmID); 
    if (acc == NULL) { 
        bankLock.readUnlock();
        return; 
    }

    // 2. Lock Account (Read) - Reading balance
    acc->lock->readLock();
    
    if (!acc->checkPassword(pass)) {
         Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - password for account id " + std::to_string(id) + " is incorrect");
         acc->lock->readUnlock();
         bankLock.readUnlock();
         return;
    }

    Log::getInstance().write(std::to_string(atmID) + ": Account " + std::to_string(id) + 
        " balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
        std::to_string(acc->balanceUSD) + " USD");
        
    acc->lock->readUnlock();
    bankLock.readUnlock();
}

// ------------------------------------------------------------
// TRANSFER 
// ------------------------------------------------------------

void Bank::transfer(int srcID, int pass, int destID, int amount, bool isDollar, int atmID) {
    // 1. Lock Bank (Read) to find accounts
    bankLock.readLock();
    
    Account* src = getAccount(srcID, atmID);
    if (src == NULL) {
        bankLock.readUnlock();
        return;
    }

    Account* dest = getAccount(destID, atmID);
    // If either failed, the error was already printed by getAccount
    if (dest == NULL) {
        bankLock.readUnlock();
        return;
    }

    // 2. Lock Accounts (ORDERED to prevent Deadlock)
    // Always lock the smaller ID first
    if (srcID < destID) {
        src->lock->writeLock();
        dest->lock->writeLock();
    } else {
        dest->lock->writeLock();
        src->lock->writeLock();
    }

    // 3. Logic Checks
    bool success = true;
    if (!src->checkPassword(pass)) {
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - password for account id " + std::to_string(srcID) + " is incorrect");
        success = false;
    } 
    else if (src->balanceILS < amount) { // Assuming ILS for simplicity, adapt for USD
        Log::getInstance().write("Error " + std::to_string(atmID) + 
            ": Your transaction failed - account id " + std::to_string(srcID) + " balance is lower than " + std::to_string(amount) + (isDollar ? " USD" : " ILS"));
        success = false;
    }

    // 4. Execute
    if (success) {
        src->balanceILS -= amount;
        dest->balanceILS += amount;
        
        Log::getInstance().write(std::to_string(atmID) + ": Transfer " + std::to_string(amount) + " ILS from account " + 
            std::to_string(srcID) + " to account " + std::to_string(destID) + " new account balance is " + 
            std::to_string(src->balanceILS) + " ILS and " + std::to_string(src->balanceUSD) + " USD new target account balance is " + 
            std::to_string(dest->balanceILS) + " ILS and " + std::to_string(dest->balanceUSD) + " USD");
    }

    // 5. Unlock
    src->lock->writeUnlock();
    dest->lock->writeUnlock();
    bankLock.readUnlock();
}

// ------------------------------------------------------------
// Rollback
// ------------------------------------------------------------

void Bank::rollback(int steps, int atmID) {
    bankLock.writeLock();
    pthread_mutex_lock(&historyLock);
    
    if ((int)history.size() > steps) {
        auto it = history.begin();
        std::advance(it, steps);
        std::map<int, Account>& snapshot = *it;

        // Clean current accounts
        for (auto& pair : accounts) delete pair.second;
        accounts.clear();

        // Restore
        for (auto& pair : snapshot) {
            Account* restoredAcc = new Account(pair.second);
            accounts.insert(std::make_pair(pair.first, restoredAcc));
        }
        Log::getInstance().write(std::to_string(atmID) + ": Rollback to " + std::to_string(steps) + " bank iterations ago was completed successfully");
    }
    else {
         Log::getInstance().write("Error " + std::to_string(atmID) + ": Cannot rollback " + std::to_string(steps) + " steps, history too short");
    }

    pthread_mutex_unlock(&historyLock);
    bankLock.writeUnlock();
}


// ------------------------------------------------------------
// ATM SHUTDOWN 
// ------------------------------------------------------------

void Bank::closeAtm(int targetID, int closerID){
    atmLock.writeLock();
    if(targetID < 1 || targetID > static_cast<int>(atmStatus.size())){
        Log::getInstance().write("Error " + std::to_string(closerID) + 
            ": Your transaction failed - ATM id " + std::to_string(targetID) + " does not exist");
        atmLock.writeUnlock();
        return;
    }
    atmStatus[targetID - 1] = closerID * -1; //set to negative to indicate request
    atmLock.writeUnlock();
}


int Bank::isAtmClosing(int atmID){
    atmLock.readLock();
    int isClosing = 0; //default not closing
    if(atmID >=1 && atmID <= static_cast<int>(atmStatus.size())){
        if(atmStatus[atmID - 1] > 0){
            isClosing = atmStatus[atmID - 1]; //return positive value (closerID)
        }
        else{
            isClosing = 0; //not closing
        }
        atmLock.readUnlock();
        return isClosing;
    }
    else{
        atmLock.readUnlock();
        Log::getInstance().write("NEED FIXING BUG, checked ATM array with illigal ATM ID: " + std::to_string(atmID));
        return isClosing;
    } 
} 
