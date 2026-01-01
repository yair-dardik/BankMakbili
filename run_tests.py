#!/usr/bin/env python3
"""
Comprehensive test runner for the concurrent bank system.
Course: 046209 - Operating Systems Structure
"""

import subprocess
import os
import sys
import time
import re
from typing import List, Tuple, Dict, Optional

# Configuration
BANK_EXECUTABLE = "./bank"
LOG_FILE = "log.txt"
TIMEOUT = 60  # seconds

class TestResult:
    def __init__(self, name: str):
        self.name = name
        self.passed = False
        self.error_message = ""
        self.log_content = ""
        self.stdout = ""
        self.stderr = ""

def run_bank(num_vip_threads: int, input_files: List[str], timeout: int = TIMEOUT) -> Tuple[int, str, str]:
    """Run the bank executable with given parameters."""
    cmd = [BANK_EXECUTABLE, str(num_vip_threads)] + input_files
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=os.path.dirname(os.path.abspath(__file__)) or "."
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"
    except Exception as e:
        return -1, "", str(e)

def read_log_file() -> str:
    """Read the log file content."""
    try:
        with open(LOG_FILE, 'r') as f:
            return f.read()
    except FileNotFoundError:
        return ""

def clean_log_file():
    """Remove the log file if it exists."""
    try:
        os.remove(LOG_FILE)
    except FileNotFoundError:
        pass

def check_log_contains(log: str, expected: List[str]) -> Tuple[bool, str]:
    """Check if log contains all expected strings."""
    missing = []
    for exp in expected:
        if exp not in log:
            missing.append(exp)
    if missing:
        return False, f"Missing in log: {missing}"
    return True, ""

def check_log_contains_pattern(log: str, patterns: List[str]) -> Tuple[bool, str]:
    """Check if log contains all expected regex patterns."""
    missing = []
    for pattern in patterns:
        if not re.search(pattern, log):
            missing.append(pattern)
    if missing:
        return False, f"Missing patterns in log: {missing}"
    return True, ""

def check_no_negative_balance(log: str) -> Tuple[bool, str]:
    """Check that no negative balances appear in log."""
    # Pattern to find balance reports
    balance_pattern = r'balance is (-?\d+) ILS and (-?\d+) USD'
    matches = re.findall(balance_pattern, log)
    for ils, usd in matches:
        if int(ils) < 0 or int(usd) < 0:
            return False, f"Negative balance found: {ils} ILS, {usd} USD"
    return True, ""

# ============== TEST CASES ==============

def test_basic_operations() -> TestResult:
    """Test basic account operations: open, deposit, withdraw, balance."""
    result = TestResult("Basic Operations")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_basic.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    if retcode != 0 and "TIMEOUT" not in stderr:
        result.error_message = f"Bank exited with code {retcode}: {stderr}"
        return result
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 1001 with password 1234",
        r"1: New account id is 1002 with password 5678",
        r"1: Account 1001 balance is \d+ ILS and \d+ USD",
        r"1: Account 1002 balance is \d+ ILS and \d+ USD",
        r"1: Account 1001 new balance is \d+ ILS and \d+ USD after 500 ILS was deposited",
        r"1: Account 1002 new balance is \d+ ILS and \d+ USD after 200 USD was deposited",
        r"1: Account 1001 new balance is \d+ ILS and \d+ USD after 300 ILS was withdrawn",
        r"1: Account 1002 new balance is \d+ ILS and \d+ USD after 100 USD was withdrawn",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    ok, msg = check_no_negative_balance(log)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_error_handling() -> TestResult:
    """Test error handling: wrong password, non-existent account."""
    result = TestResult("Error Handling")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_errors.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 2001",
        r"Error 1:.*password for account id 2001 is incorrect",
        r"Error 1:.*password for account id 2001 is incorrect",
        r"Error 1:.*password for account id 2001 is incorrect",
        r"Error 1:.*account id 9999 does not exist",
        r"Error 1:.*account id 9999 does not exist",
        r"Error 1:.*account id 9999 does not exist",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_transfers() -> TestResult:
    """Test money transfers between accounts."""
    result = TestResult("Transfers")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_transfer.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 3001",
        r"1: New account id is 3002",
        r"1: Transfer 200 ILS from account 3001 to account 3002",
        r"1: Transfer 100 USD from account 3002 to account 3001",
        r"Error 1:.*account id 3001.*balance is lower than 5000 ILS",
        r"Error 1:.*password for account id 3001 is incorrect",
        r"Error 1:.*account id 9999 does not exist",
        r"Error 1:.*account id 9999 does not exist",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_currency_exchange() -> TestResult:
    """Test currency exchange operations."""
    result = TestResult("Currency Exchange")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_exchange.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 4001",
        r"1: Account 4001 new balance is \d+ ILS and \d+ USD after 500 ILS was exchanged",
        r"1: Account 4001 new balance is \d+ ILS and \d+ USD after 150 USD was exchanged",
        r"Error 1:.*balance.*is lower than 10000 ILS",
        r"Error 1:.*password for account id 4001 is incorrect",
        r"Error 1:.*account id 9999 does not exist",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_close_account() -> TestResult:
    """Test closing accounts."""
    result = TestResult("Close Account")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_close_account.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    # Balance may change due to commissions, so use flexible patterns
    expected_patterns = [
        r"1: New account id is 5001 with password 1234",
        r"1: Account 5001 is now closed\. Balance was \d+ ILS and \d+ USD",
        r"1: New account id is 5001 with password 5678",
        r"1: Account 5001 balance is \d+ ILS and \d+ USD",
        r"Error 1:.*password for account id 5001 is incorrect",
        r"Error 1:.*account id 9999 does not exist",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_insufficient_funds() -> TestResult:
    """Test insufficient funds errors."""
    result = TestResult("Insufficient Funds")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_insufficient.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 6001",
        r"Error 1:.*account id 6001 balance is \d+ ILS and \d+ USD is lower than 10000 ILS",
        r"1: Account 6001 new balance is \d+ ILS and \d+ USD after 100 ILS was deposited",
        r"Error 1:.*account id 6001 balance is \d+ ILS and \d+ USD is lower than 2000 USD",
        r"1: Account 6001 new balance is \d+ ILS and \d+ USD after 50 USD was deposited",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    ok, msg = check_no_negative_balance(log)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_concurrent_atms() -> TestResult:
    """Test concurrent ATM operations."""
    result = TestResult("Concurrent ATMs")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_concurrent1.txt", "tests/test_concurrent2.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    # Both ATMs should create accounts and perform operations
    expected_patterns = [
        r"1: New account id is 7001",
        r"1: New account id is 7002",
        r"2: New account id is 7003",
        r"2: New account id is 7004",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    ok, msg = check_no_negative_balance(log)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_multi_atm_transfers() -> TestResult:
    """Test transfers between accounts created by different ATMs."""
    result = TestResult("Multi-ATM Transfers")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_atm1.txt", "tests/test_atm2.txt", "tests/test_atm3.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"\d: New account id is 8001",
        r"\d: New account id is 8002",
        r"\d: New account id is 8003",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    ok, msg = check_no_negative_balance(log)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_persistent_commands() -> TestResult:
    """Test persistent command modifier."""
    result = TestResult("Persistent Commands")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_persistent.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 9001",
        r"1: Account 9001 new balance is \d+ ILS and \d+ USD after 100 ILS was deposited",
        r"1: Account 9001 new balance is \d+ ILS and \d+ USD after 50 ILS was withdrawn",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_vip_commands() -> TestResult:
    """Test VIP command handling."""
    result = TestResult("VIP Commands")
    clean_log_file()
    
    # Use 2 VIP threads
    retcode, stdout, stderr = run_bank(2, ["tests/test_vip.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    # Account should be created and VIP deposits should eventually execute
    expected_patterns = [
        r"1: New account id is 10001",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_investment() -> TestResult:
    """Test investment operations."""
    result = TestResult("Investment")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_invest.txt"], timeout=30)
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"1: New account id is 11001",
        r"1: Account 11001 new balance is 9000 ILS and 5000 USD after 1000 ILS was invested for 6 sec",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_commission_charging() -> TestResult:
    """Test that commissions are charged."""
    result = TestResult("Commission Charging")
    clean_log_file()
    
    # Use a longer-running test to see commissions
    retcode, stdout, stderr = run_bank(0, ["tests/test_basic.txt"], timeout=30)
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    expected_patterns = [
        r"Bank: commissions of \d+ % were charged",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_illegal_arguments() -> TestResult:
    """Test error handling for illegal arguments."""
    result = TestResult("Illegal Arguments")
    
    # Test with no arguments
    retcode, stdout, stderr = run_bank(0, [], timeout=5)
    # This will fail to run properly, that's expected
    
    # Test with non-existent file
    retcode, stdout, stderr = run_bank(0, ["nonexistent_file.txt"], timeout=5)
    result.stdout = stdout
    result.stderr = stderr
    
    if "Bank error: illegal arguments" in stderr or retcode != 0:
        result.passed = True
    else:
        result.error_message = "Expected 'illegal arguments' error"
    
    return result

def test_status_printing() -> TestResult:
    """Test that bank status is printed to stdout."""
    result = TestResult("Status Printing")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_basic.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    # Check stdout contains bank status
    if "Current Bank Status" in stdout or "Account" in stdout:
        result.passed = True
    else:
        result.error_message = "Bank status not found in stdout"
    
    return result

def test_close_atm() -> TestResult:
    """Test closing ATM functionality."""
    result = TestResult("Close ATM")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, ["tests/test_close_atm1.txt", "tests/test_close_atm2.txt"])
    result.stdout = stdout
    result.stderr = stderr
    
    log = read_log_file()
    result.log_content = log
    
    # ATM 2 should request to close ATM 1
    expected_patterns = [
        r"1: New account id is 13001",
        r"Bank: ATM 2 closed 1 successfully",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def test_stress_multi_atm() -> TestResult:
    """Stress test with 4 ATMs doing concurrent operations."""
    result = TestResult("Stress Multi-ATM")
    clean_log_file()
    
    retcode, stdout, stderr = run_bank(0, [
        "tests/stress1.txt", 
        "tests/stress2.txt", 
        "tests/stress3.txt", 
        "tests/stress4.txt"
    ], timeout=120)
    result.stdout = stdout
    result.stderr = stderr
    
    if "TIMEOUT" in stderr:
        result.error_message = "Test timed out"
        return result
    
    log = read_log_file()
    result.log_content = log
    
    # All 4 accounts should be created
    expected_patterns = [
        r"\d: New account id is 20001",
        r"\d: New account id is 20002",
        r"\d: New account id is 20003",
        r"\d: New account id is 20004",
    ]
    
    ok, msg = check_log_contains_pattern(log, expected_patterns)
    if not ok:
        result.error_message = msg
        return result
    
    ok, msg = check_no_negative_balance(log)
    if not ok:
        result.error_message = msg
        return result
    
    result.passed = True
    return result

def run_all_tests() -> List[TestResult]:
    """Run all tests and return results."""
    tests = [
        test_basic_operations,
        test_error_handling,
        test_transfers,
        test_currency_exchange,
        test_close_account,
        test_insufficient_funds,
        test_concurrent_atms,
        test_multi_atm_transfers,
        test_persistent_commands,
        test_vip_commands,
        test_investment,
        test_commission_charging,
        test_illegal_arguments,
        test_status_printing,
        test_close_atm,
        test_stress_multi_atm,
    ]
    
    results = []
    for test_func in tests:
        print(f"Running: {test_func.__name__}...", end=" ", flush=True)
        try:
            result = test_func()
            results.append(result)
            if result.passed:
                print("PASSED")
            else:
                print(f"FAILED: {result.error_message}")
        except Exception as e:
            result = TestResult(test_func.__name__)
            result.error_message = str(e)
            results.append(result)
            print(f"ERROR: {e}")
    
    return results

def print_summary(results: List[TestResult]):
    """Print test summary."""
    passed = sum(1 for r in results if r.passed)
    total = len(results)
    
    print("\n" + "=" * 60)
    print(f"TEST SUMMARY: {passed}/{total} tests passed")
    print("=" * 60)
    
    if passed < total:
        print("\nFailed tests:")
        for r in results:
            if not r.passed:
                print(f"  - {r.name}: {r.error_message}")
                if r.log_content:
                    print(f"    Log preview: {r.log_content[:200]}...")

def main():
    # Check if bank executable exists
    if not os.path.exists(BANK_EXECUTABLE):
        print(f"Error: {BANK_EXECUTABLE} not found. Run 'make' first.")
        sys.exit(1)
    
    # Check if tests directory exists
    if not os.path.exists("tests"):
        print("Error: tests directory not found.")
        sys.exit(1)
    
    print("=" * 60)
    print("Concurrent Bank System - Test Suite")
    print("=" * 60)
    print()
    
    results = run_all_tests()
    print_summary(results)
    
    # Exit with non-zero if any test failed
    if any(not r.passed for r in results):
        sys.exit(1)

if __name__ == "__main__":
    main()
