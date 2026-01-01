#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "RWLock.h"

class Account {
public:
    int id;
    int password;
    int balanceILS;
    int balanceUSD;
    
    RWLock* lock; 

    Account(int id, int password, int amountILS, int amountUSD);
    Account(const Account& other); // Copy constructor
    ~Account();

    // Helper to check password without locking (caller must hold lock)
    bool checkPassword(int pwd) const;
};

#endif