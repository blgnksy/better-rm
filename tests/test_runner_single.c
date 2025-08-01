#include <check.h>
#include <stdio.h>
#include <stdlib.h>

// Declare test suite creation functions
#define DECLARE_SUITE(name) Suite *name##_suite(void);

DECLARE_SUITE(test_main)
DECLARE_SUITE(test_path_operations)
DECLARE_SUITE(test_protected_dirs)
DECLARE_SUITE(test_trash_operations)
DECLARE_SUITE(test_remove_operations)
DECLARE_SUITE(test_config_parser)

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    // Create the appropriate suite based on compile-time definition
#if defined(TEST_SUITE)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CONCAT(a, b) a##b
#define SUITE_FUNC(name) CONCAT(name, _suite)

    s = SUITE_FUNC(TEST_SUITE)();
#else
    fprintf(stderr, "No test suite specified\n");
    return EXIT_FAILURE;
#endif

    sr = srunner_create(s);

    // Run tests with verbose output
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
