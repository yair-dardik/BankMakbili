# Bank HW2 Test Suite (smoke + functional)

## How to run
From your project root (where Makefile and sources are), run:

    ./bank_tests/run_tests.sh

The script will:
- build the project (make clean && make)
- run multiple test cases (each provides ATM input files)
- store outputs under bank_tests/out/

## Notes
- Some tests assume you implemented:
  - status printing thread ("Current Bank Status")
  - VIP handling (commands with "VIP=NN")
  - commissions thread ("Bank: commissions of ...")
  - rollback that logs success and actually restores a previous snapshot
  - close ATM request (ATM2 stops processing further commands)

If your implementation differs in log wording, adjust patterns in cases/*/checks.txt.
