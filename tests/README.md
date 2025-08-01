# Unit Tests for better-rm

This directory contains comprehensive unit tests for better-rm using the Check unit testing framework for C.

## Test Structure

- **test_main.c** - Basic initialization and environment tests
- **test_path_operations.c** - Path resolution and manipulation tests
- **test_protected_dirs.c** - Protected directory functionality tests
- **test_trash_operations.c** - Trash/recovery feature tests
- **test_remove_operations.c** - File and directory removal tests
- **test_config_parser.c** - Configuration file parsing tests
- **test_runner.c** - Main test runner that executes all test suites
- **test_runner_single.c** - Runner for individual test suites (debugging)

## Prerequisites

### Install Check Framework

#### Debian/Ubuntu:
```bash
sudo apt-get install check
```

#### Fedora/RHEL:
```bash
sudo dnf install check-devel
```

#### macOS:
```bash
brew install check
```

#### From Source:
```bash
wget https://github.com/libcheck/check/releases/download/0.15.2/check-0.15.2.tar.gz
tar xvfz check-0.15.2.tar.gz
cd check-0.15.2
./configure
make
sudo make install
```

## Running Tests

### Quick Run (using script):
```bash
./scripts/run-tests.sh
```

### Manual Build and Run:
```bash
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON
make
ctest --output-on-failure
```

### Run All Tests Directly:
```bash
./build/tests/test_runner
```

### Run Specific Test Suite:
```bash
# Run individual test suite for debugging
./build/tests/test_path_operations_individual
```

### Run with Valgrind (memory leak detection):
```bash
valgrind --leak-check=full ./build/tests/test_runner
```

## Test Coverage

### Generate Coverage Report:
```bash
mkdir build-coverage
cd build-coverage
cmake .. -DBUILD_TESTS=ON -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage"
make
./tests/test_runner
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

Open `coverage-report/index.html` in your browser to view the coverage report.

## Writing New Tests

### Test Template:
```c
#include <check.h>
#include <stdlib.h>
#include <string.h>

// Function declarations from main.c
void function_to_test(const char* input);

// Test fixture data
static char* test_data = NULL;

static void setup(void) {
    // Test setup - run before each test
    test_data = strdup("test");
}

static void teardown(void) {
    // Cleanup - run after each test
    free(test_data);
    test_data = NULL;
}

// Individual test
START_TEST(test_function_basic) {
    // Test code
    ck_assert_str_eq(test_data, "test");
    ck_assert_int_eq(some_function(), 0);
}
END_TEST

// Create test suite
Suite* my_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("My Test Suite");
    tc_core = tcase_create("Core");

    // Add fixtures
    tcase_add_checked_fixture(tc_core, setup, teardown);

    // Add tests
    tcase_add_test(tc_core, test_function_basic);

    suite_add_tcase(s, tc_core);

    return s;
}
```

### Check Assertions:

- `ck_assert(expr)` - Assert expression is true
- `ck_assert_int_eq(a, b)` - Assert integers are equal
- `ck_assert_str_eq(a, b)` - Assert strings are equal
- `ck_assert_ptr_null(ptr)` - Assert pointer is NULL
- `ck_assert_ptr_nonnull(ptr)` - Assert pointer is not NULL
- `ck_abort_msg("message")` - Fail test with message

### Best Practices:

1. **Isolation**: Each test should be independent
2. **Cleanup**: Always clean up created files/directories in teardown
3. **Assertions**: Use specific assertions (e.g., `ck_assert_int_eq`) over generic `ck_assert`
4. **Naming**: Use descriptive test names that explain what is being tested
5. **Coverage**: Aim for both positive and negative test cases

## Test Categories

### Unit Tests
- Test individual functions in isolation
- Fast execution
- Use temporary directories for file operations

### Integration Tests
- Test interaction between components
- Test actual file system operations
- May take longer to execute

## Debugging Failed Tests

### Get Detailed Output:
```bash
# Run with verbose output
CK_VERBOSITY=verbose ./build/tests/test_runner

# Run specific test
CK_RUN_SUITE="Path Operations" ./build/tests/test_runner
CK_RUN_CASE="Core" ./build/tests/test_runner
```

### Debug with GDB:
```bash
gdb ./build/tests/test_runner
(gdb) run
(gdb) break test_function_name
```

### Check Memory Leaks:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./build/tests/test_runner
```

## CI Integration

Tests are automatically run in GitHub Actions on:
- Every push
- Every pull request
- Multiple compiler configurations (GCC, Clang)
- Multiple build types (Debug, Release)
- Memory leak checking with Valgrind
- Code coverage reporting to Codecov
