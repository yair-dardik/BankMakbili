import os
import random

# Configuration
NUM_ATMS = 50           # ATMs NUM 
NUM_ACCOUNTS = 4        # Accounts num
OPERATIONS_PER_FILE = 20 # Number of operations per ATM
INIT_BALANCE = 1000
PASSWORD = 1
OUTPUT_DIR = "stress_tests"

def ensure_dir(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def get_random_currency():
    return "ILS" if random.random() < 0.5 else "USD"

def generate_pair_ops():
    
    op_type = random.choice(['DW', 'T']) 
    
    amount = random.randint(10, 50)
    acc1 = random.randint(1, NUM_ACCOUNTS)
    acc2 = random.randint(1, NUM_ACCOUNTS)
    while acc1 == acc2:
        acc2 = random.randint(1, NUM_ACCOUNTS)
    
    currency = get_random_currency()
    
    lines = []
    
    if op_type == 'DW':
        
        lines.append("D {} {} {} {}".format(acc1, PASSWORD, amount, currency))
        lines.append("W {} {} {} {}".format(acc1, PASSWORD, amount, currency))
        
    elif op_type == 'T':
        lines.append("T {} {} {} {} {}".format(acc1, PASSWORD, acc2, amount, currency))
        lines.append("T {} {} {} {} {}".format(acc2, PASSWORD, acc1, amount, currency))
        
    return lines

def main():
    ensure_dir(OUTPUT_DIR)
    
    atm_filenames = []
    
    for i in range(1, NUM_ATMS + 1):
        
        filename = os.path.join(OUTPUT_DIR, "atm{}.txt".format(i))
        atm_filenames.append(filename)
        
        with open(filename, "w") as f:
            if i == 1:
                for acc_id in range(1, NUM_ACCOUNTS + 1):
                   
                    f.write("O {} {} {} {}\n".format(acc_id, PASSWORD, INIT_BALANCE, INIT_BALANCE))
                f.write("S 200\n") 
            else:
                f.write("S 1000\n")
            
            for _ in range(OPERATIONS_PER_FILE):
                ops = generate_pair_ops()
                for op in ops:
                    f.write(op + "\n")
            
            if i == NUM_ATMS:
                f.write("S 2000\n")
                for acc_id in range(1, NUM_ACCOUNTS + 1):
                   
                    f.write("B {} {}\n".format(acc_id, PASSWORD))

    print("Generated {} files in folder {}".format(NUM_ATMS, OUTPUT_DIR))
    
    args = " ".join(atm_filenames)
    print("./bank 4 {}".format(args))

    with open("run_stress_test.sh", "w") as f:
        f.write("#!/bin/bash\n")
        f.write("./bank 0 {} > output_stress.txt 2>&1\n".format(args))
        f.write("echo 'Test finished. Check output_stress.txt'\n")
        f.write("grep 'Balance' output_stress.txt | tail -n 4\n")

if __name__ == "__main__":
    main()