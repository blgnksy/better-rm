#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Function declarations from main.c
char *resolve_path(const char *path);
char *generate_trash_name(const char *original_path, const char *trash_dir);

// Test fixture data
static char *test_dir = NULL;

static void setup(void) {
    char temp_template[] = "/tmp/better_rm_path_test_XXXXXX";
    test_dir = mkdtemp(temp_template);
    ck_assert_ptr_nonnull(test_dir);
    test_dir = strdup(test_dir);

    // Change to test directory
    ck_assert_int_eq(chdir(test_dir), 0);
}

static void teardown(void) {
    chdir("/");
    if (test_dir) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
        system(cmd);
        free(test_dir);
        test_dir = NULL;
    }
}

// Test absolute path resolution
START_TEST(test_resolve_absolute_path) {
    char *resolved = resolve_path("/usr/bin");
    ck_assert_ptr_nonnull(resolved);
    ck_assert_str_eq(resolved, "/usr/bin");
    free(resolved);
}
END_TEST

// Test relative path resolution
START_TEST(test_resolve_relative_path) {
    // Create a subdirectory
    const char *subdir = "testdir";
    mkdir(subdir, 0755);

    char *resolved = resolve_path(subdir);
    ck_assert_ptr_nonnull(resolved);

    char expected[256];
    snprintf(expected, sizeof(expected), "%s/%s", test_dir, subdir);
    ck_assert_str_eq(resolved, expected);
    free(resolved);
}
END_TEST

// Test symlink resolution
START_TEST(test_resolve_symlink) {
    // Create a file and symlink
    const char *target = "target_file";
    const char *link = "test_link";

    // Create target file
    FILE *f = fopen(target, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    // Create symlink
    ck_assert_int_eq(symlink(target, link), 0);

    char *resolved = resolve_path(link);
    ck_assert_ptr_nonnull(resolved);

    // Should resolve to the actual file
    char expected[256];
    snprintf(expected, sizeof(expected), "%s/%s", test_dir, target);
    ck_assert_str_eq(resolved, expected);
    free(resolved);
}
END_TEST

// Test non-existent path
START_TEST(test_resolve_nonexistent_path) {
    char *resolved = resolve_path("does_not_exist");
    // Should still return something (constructed path)
    ck_assert_ptr_nonnull(resolved);

    char expected[256];
    snprintf(expected, sizeof(expected), "%s/does_not_exist", test_dir);
    ck_assert_str_eq(resolved, expected);
    free(resolved);
}
END_TEST

// Test trash name generation
START_TEST(test_generate_trash_name) {
    const char *original = "/home/user/document.txt";
    const char *trash_dir_path = "/home/user/.Trash";

    char *trash_name = generate_trash_name(original, trash_dir_path);
    ck_assert_ptr_nonnull(trash_name);

    // Should start with trash directory
    ck_assert(strstr(trash_name, trash_dir_path) == trash_name);

    // Should contain original filename
    ck_assert_ptr_nonnull(strstr(trash_name, "document.txt"));

    // Should contain timestamp
    ck_assert_ptr_nonnull(strstr(trash_name, ".20")); // Year starts with 20

    // Should contain PID
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), ".%d", getpid());
    ck_assert_ptr_nonnull(strstr(trash_name, pid_str));
}
END_TEST


// Test path with trailing slashes
START_TEST(test_resolve_path_trailing_slash) {
    char *resolved = resolve_path("/usr/bin/");
    ck_assert_ptr_nonnull(resolved);

    // realpath typically removes trailing slashes
    ck_assert_str_eq(resolved, "/usr/bin");
    free(resolved);
}
END_TEST

// Test current directory
START_TEST(test_resolve_current_dir) {
    char *resolved = resolve_path(".");
    ck_assert_ptr_nonnull(resolved);
    ck_assert_str_eq(resolved, test_dir);
    free(resolved);
}
END_TEST

// Test parent directory
START_TEST(test_resolve_parent_dir) {
    char *resolved = resolve_path("..");
    ck_assert_ptr_nonnull(resolved);
    ck_assert_str_eq(resolved, "/tmp");
    free(resolved);
}
END_TEST

// Create test suite
Suite *test_path_operations_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Path Operations");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_resolve_absolute_path);
    tcase_add_test(tc_core, test_resolve_relative_path);
    tcase_add_test(tc_core, test_resolve_symlink);
    tcase_add_test(tc_core, test_resolve_nonexistent_path);
    tcase_add_test(tc_core, test_generate_trash_name);
    tcase_add_test(tc_core, test_resolve_path_trailing_slash);
    tcase_add_test(tc_core, test_resolve_current_dir);
    tcase_add_test(tc_core, test_resolve_parent_dir);

    suite_add_tcase(s, tc_core);

    return s;
}
