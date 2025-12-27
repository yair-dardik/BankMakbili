#include "Account.h"

Account::Account(int id, int pass, int ils, int usd) 
    : id(id), password(pass), balanceILS(ils), balanceUSD(usd) {
    lock = new RWLock();
}

Account::Account(const Account& other) 
    : id(other.id), password(other.password), balanceILS(other.balanceILS), 
      balanceUSD(other.balanceUSD) {
    // When copying (e.g., for snapshot), we allocate a NEW lock 
    // (though snapshots won't really use it)
    lock = new RWLock(); 
}

Account::~Account() {
    delete lock;
}

bool Account::checkPassword(int pwd) const {
    return password == pwd;
}