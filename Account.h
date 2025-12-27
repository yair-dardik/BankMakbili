#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "RWLock.h"

class Account {
public:
    int id;
    int password;
    int balanceILS;
    int balanceUSD;
    
    RWLock* lock; //TODO no pointer

    Account(int id, int password, int amountILS, int amountUSD);
    Account(const Account& other); // Copy constructor
    //TODO : we will need a = as well
    ~Account();

    // Helper to check password without locking (caller must hold lock)
    bool checkPassword(int pwd) const;
};

#endif