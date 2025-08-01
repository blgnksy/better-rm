#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Declare the C functions we're testing
#include "../include/version.h"

// Function declarations from main.c
void init_protected_dirs(void);
void load_configs(void);
char *get_trash_dir(void);
int ensure_trash_dir(const char *trash_dir);

// Test fixture data
static char *test_dir = NULL;
static char *original_home = NULL;

// Setup function
static void setup(void) {
    // Save original HOME
    const char *home = getenv("HOME");
    if (home) {
        original_home = strdup(home);
    }

    // Create temporary test directory
    char temp_template[] = "/tmp/better_rm_test_XXXXXX";
    test_dir = mkdtemp(temp_template);
    ck_assert_ptr_nonnull(test_dir);
    test_dir = strdup(test_dir);

    // Set HOME to test directory
    setenv("HOME", test_dir, 1);

    // Initialize protected directories
    init_protected_dirs();
}

// Teardown function
static void teardown(void) {
    // Restore original HOME
    if (original_home) {
        setenv("HOME", original_home, 1);
        free(original_home);
        original_home = NULL;
    }

    // Clean up test directory
    if (test_dir) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
        system(cmd);
        free(test_dir);
        test_dir = NULL;
    }
}

// Test initialization
START_TEST(test_init_protected_dirs) {
    // init_protected_dirs is called in setup
    // This test verifies it doesn't crash
    ck_assert(1);
}
END_TEST

// Test trash directory resolution
START_TEST(test_get_trash_dir) {
    char *trash_dir = get_trash_dir();
    ck_assert_ptr_nonnull(trash_dir);

    // Should be in test HOME directory
    char expected[256];
    snprintf(expected, sizeof(expected), "%s/.Trash", test_dir);
    ck_assert_str_eq(trash_dir, expected);
}
END_TEST

// Test trash directory with environment variable
START_TEST(test_get_trash_dir_with_env) {
    char custom_trash[256];
    snprintf(custom_trash, sizeof(custom_trash), "%s/custom_trash", test_dir);
    setenv("BETTER_RM_TRASH", custom_trash, 1);

    char *trash_dir = get_trash_dir();
    ck_assert_ptr_nonnull(trash_dir);
    ck_assert_str_eq(trash_dir, custom_trash);

    unsetenv("BETTER_RM_TRASH");
}
END_TEST

// Test trash directory creation
START_TEST(test_ensure_trash_dir) {
    char trash_path[256];
    snprintf(trash_path, sizeof(trash_path), "%s/.Trash", test_dir);

    // Directory shouldn't exist yet
    struct stat st;
    ck_assert_int_ne(stat(trash_path, &st), 0);

    // Create it
    ck_assert_int_eq(ensure_trash_dir(trash_path), 0);

    // Now it should exist
    ck_assert_int_eq(stat(trash_path, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Should handle existing directory
    ck_assert_int_eq(ensure_trash_dir(trash_path), 0);
}
END_TEST

// Test trash directory creation failure
START_TEST(test_ensure_trash_dir_failure) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/not_a_dir", test_dir);

    // Create a file where trash dir should be
    FILE *f = fopen(file_path, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    // Should fail since it's a file
    ck_assert_int_ne(ensure_trash_dir(file_path), 0);
}
END_TEST

// Test version string
START_TEST(test_version_defined) {
    ck_assert_ptr_nonnull(VERSION);
    ck_assert_int_gt(strlen(VERSION), 0);
}
END_TEST

// Create test suite
Suite *test_main_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Main");

    /* Core test case */
    tc_core = tcase_create("Core");

    // Add setup and teardown
    tcase_add_checked_fixture(tc_core, setup, teardown);

    // Add tests
    tcase_add_test(tc_core, test_init_protected_dirs);
    tcase_add_test(tc_core, test_get_trash_dir);
    tcase_add_test(tc_core, test_get_trash_dir_with_env);
    tcase_add_test(tc_core, test_ensure_trash_dir);
    tcase_add_test(tc_core, test_ensure_trash_dir_failure);
    tcase_add_test(tc_core, test_version_defined);

    suite_add_tcase(s, tc_core);

    return s;
}
