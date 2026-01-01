#ifndef BANK_H
#define BANK_H

#include <map>
#include <vector>
#include <list>
#include <pthread.h>
#include "Account.h"
#include "RWLock.h"

class Bank {
private:


    std::map<int, Account*> accounts;
    RWLock bankLock; 
    
    std::list<std::map<int, Account> > history; 
    pthread_mutex_t historyLock;
    
    // 0 = Running
    // Negative = Requested (waiting for bank)
    // Positive = Shutdown (approved)
    std::vector<int> atmStatus;  //to get ATM i, enter atmStatus[i-1]
    RWLock atmLock; 

    pthread_t maintenanceThread;
    

    Bank(); 
    Account* getAccount(int id, int atmID);

public:
    bool bankIsRunning;
    static Bank& getInstance();
    void initWrappers(int numATMs);
    void runMaintenance();
    
    // ATM Commands
    void createAccount(int id, int pass, int ils, int usd, int atmID);
    void closeAccount(int id, int pass, int atmID);
    void deposit(int id, int pass, int amount, bool isDollar, bool isInvest, int atmID);
    void withdraw(int id, int pass, int amount,bool isDollar, bool isInvest, int atmID);
    void getBalance(int id, int pass, int atmID);
    void exchange(int id, int pass, int amount,int isILStoDollar, int atmID);
    
    // Complex Commands (To be implemented below)
    void transfer(int srcID, int pass, int destID, int amount, bool isDollar, int atmID);
    void rollback(int steps, int atmID);
    
    // Stock Market
    void invest(int id, int pass, int amount, bool isDollar, int time, int atmID);

    // Shutdown Logic
    void closeAtm(int targetID, int closerID); 
    int isAtmClosing(int atmID);   
    
    // Admin
    void takeSnapshot();
    void printStatus();
    void collectCommission();

    
};

#endif