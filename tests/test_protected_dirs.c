#include <check.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Function declarations from main.c
void init_protected_dirs(void);
void load_config_file(const char *filename);
bool is_protected(const char *path);
bool is_root_with_preserve(const char *path, const struct Options *opts);

// Options struct from main.c
struct Options {
    bool recursive;
    bool force;
    bool verbose;
    bool interactive;
    bool dry_run;
    bool preserve_root;
    bool one_file_system;
    bool use_trash;
    bool no_preserve_root;
    char *trash_dir;
};

// Access to protected dirs for testing
extern char *protected_dirs[];
extern int protected_count;

static void setup(void) {
    // Reset protected directories
    for (int i = 0; i < protected_count; i++) {
        free(protected_dirs[i]);
    }
    protected_count = 0;
    init_protected_dirs();
}

static void teardown(void) {
    // Clean up allocated memory
    for (int i = 0; i < protected_count; i++) {
        free(protected_dirs[i]);
    }
    protected_count = 0;
}

// Test non-protected directories
START_TEST(test_non_protected_dirs) {
    ck_assert(!is_protected("/tmp"));
    ck_assert(!is_protected("/home/user"));
    ck_assert(!is_protected("/usr/local"));
    ck_assert(!is_protected("/var/tmp"));
    ck_assert(!is_protected("/mnt"));
}
END_TEST

// Test protected directory with trailing slash
START_TEST(test_protected_dir_trailing_slash) {
    ck_assert(is_protected("/usr/"));
    ck_assert(is_protected("/home/"));
    ck_assert(is_protected("/etc/"));
}
END_TEST

// Test loading custom config
START_TEST(test_load_custom_config) {
    // Create temporary config file
    char temp_config[] = "/tmp/test_config_XXXXXX";
    int fd = mkstemp(temp_config);
    ck_assert_int_ne(fd, -1);

    // Write test config
    const char *config_content = "# Test config\n"
                                 "protect=/custom/dir1\n"
                                 "protect=/custom/dir2\n"
                                 "\n"
                                 "# Comment\n"
                                 "protect=/opt/important\n";

    write(fd, config_content, strlen(config_content));
    close(fd);

    // Load config
    int old_count = protected_count;
    load_config_file(temp_config);

    // Should have added 3 directories
    ck_assert_int_eq(protected_count, old_count + 3);

    // Test if custom directories are protected
    ck_assert(is_protected("/custom/dir1"));
    ck_assert(is_protected("/custom/dir2"));
    ck_assert(is_protected("/opt/important"));

    // Clean up
    unlink(temp_config);
}
END_TEST

// Test config with invalid lines
START_TEST(test_load_config_invalid_lines) {
    char temp_config[] = "/tmp/test_config_XXXXXX";
    int fd = mkstemp(temp_config);
    ck_assert_int_ne(fd, -1);

    const char *config_content = "# Comment\n"
                                 "\n"
                                 "invalid line\n"
                                 "protect=/valid/dir\n"
                                 "also invalid\n"
                                 "trash_dir=/tmp/trash\n"; // Should be ignored

    write(fd, config_content, strlen(config_content));
    close(fd);

    int old_count = protected_count;
    load_config_file(temp_config);

    // Should only add 1 valid directory
    ck_assert_int_eq(protected_count, old_count + 1);
    ck_assert(is_protected("/valid/dir"));

    unlink(temp_config);
}
END_TEST

// Test non-existent config file
START_TEST(test_load_nonexistent_config) {
    int old_count = protected_count;
    load_config_file("/non/existent/config");

    // Should not change count
    ck_assert_int_eq(protected_count, old_count);
}
END_TEST

// Test preserve root functionality
START_TEST(test_preserve_root_default) {
    struct Options opts = {0};
    opts.preserve_root = true;

    ck_assert(is_root_with_preserve("/", &opts));
    ck_assert(!is_root_with_preserve("/home", &opts));
    ck_assert(!is_root_with_preserve("/usr", &opts));
}
END_TEST

// Test no preserve root
START_TEST(test_no_preserve_root) {
    struct Options opts = {0};
    opts.no_preserve_root = true;

    ck_assert(!is_root_with_preserve("/", &opts));
}
END_TEST

// Test max protected directories
START_TEST(test_max_protected_dirs) {
    // Fill up to max
    while (protected_count < 100) {
        char dir[32];
        snprintf(dir, sizeof(dir), "/test%d", protected_count);
        protected_dirs[protected_count++] = strdup(dir);
    }

    ck_assert_int_eq(protected_count, 100);

    // Try to load more from config - should be ignored
    char temp_config[] = "/tmp/test_config_XXXXXX";
    int fd = mkstemp(temp_config);
    ck_assert_int_ne(fd, -1);

    write(fd, "protect=/should/not/be/added\n", 30);
    close(fd);

    load_config_file(temp_config);

    // Count should not change
    ck_assert_int_eq(protected_count, 100);
    ck_assert(!is_protected("/should/not/be/added"));

    unlink(temp_config);
}
END_TEST

// Test case sensitivity
START_TEST(test_case_sensitivity) {
    ck_assert(is_protected("/usr"));
    ck_assert(!is_protected("/USR"));
    ck_assert(!is_protected("/Usr"));
}
END_TEST

// Create test suite
Suite *test_protected_dirs_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Protected Directories");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_non_protected_dirs);
    tcase_add_test(tc_core, test_protected_dir_trailing_slash);
    tcase_add_test(tc_core, test_load_custom_config);
    tcase_add_test(tc_core, test_load_config_invalid_lines);
    tcase_add_test(tc_core, test_load_nonexistent_config);
    tcase_add_test(tc_core, test_preserve_root_default);
    tcase_add_test(tc_core, test_no_preserve_root);
    tcase_add_test(tc_core, test_max_protected_dirs);
    tcase_add_test(tc_core, test_case_sensitivity);

    suite_add_tcase(s, tc_core);

    return s;
}
