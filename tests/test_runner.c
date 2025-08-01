#include <check.h>
#include <stdio.h>
#include <stdlib.h>

// Declare test suite creation functions
Suite *test_main_suite(void);
Suite *test_path_operations_suite(void);
Suite *test_protected_dirs_suite(void);
Suite *test_trash_operations_suite(void);
Suite *test_remove_operations_suite(void);
Suite *test_config_parser_suite(void);

int main(void) {
    int number_failed;
    SRunner *sr;

    // Create suite runner
    sr = srunner_create(test_main_suite());

    // Add other test suites
    srunner_add_suite(sr, test_path_operations_suite());
    srunner_add_suite(sr, test_protected_dirs_suite());
    srunner_add_suite(sr, test_trash_operations_suite());
    srunner_add_suite(sr, test_remove_operations_suite());
    srunner_add_suite(sr, test_config_parser_suite());

    // Run tests
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
