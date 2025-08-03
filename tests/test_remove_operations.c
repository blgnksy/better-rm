#include <check.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Function declarations from main.c
int safe_remove(const char *path, const struct Options *opts);
int remove_directory(const char *path, const struct Options *opts);
void init_protected_dirs(void);
int ensure_trash_dir(const char *trash_dir);

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
    const char *trash_dir;
};

// Test fixture data
static char *test_dir = NULL;
static char *trash_dir = NULL;
static struct Options default_opts;

static void setup(void) {
    // Create test directory
    char temp_template[] = "/tmp/better_rm_remove_test_XXXXXX";
    test_dir = mkdtemp(temp_template);
    ck_assert_ptr_nonnull(test_dir);
    test_dir = strdup(test_dir);

    // Create trash directory
    trash_dir = malloc(strlen(test_dir) + 10);
    sprintf(trash_dir, "%s/.Trash", test_dir);
    ensure_trash_dir(trash_dir);

    // Change to test directory
    ck_assert_int_eq(chdir(test_dir), 0);

    // Initialize protected dirs
    init_protected_dirs();

    // Set default options
    memset(&default_opts, 0, sizeof(default_opts));
    default_opts.preserve_root = true;
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
    if (trash_dir) {
        free(trash_dir);
        trash_dir = NULL;
    }
}

static void create_test_file(const char *name, const char *content) {
    FILE *file = fopen(name, "w");
    ck_assert_ptr_nonnull(file);
    if (content) {
        fprintf(file, "%s", content);
    }
    fclose(file);
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Test removing a simple file
START_TEST(test_remove_file) {
    const char *test_file = "test_file.txt";
    create_test_file(test_file, "test content");

    ck_assert_int_eq(safe_remove(test_file, &default_opts), 0);
    ck_assert(!file_exists(test_file));
}
END_TEST

// Test removing non-existent file
START_TEST(test_remove_nonexistent_file) { ck_assert_int_ne(safe_remove("does_not_exist.txt", &default_opts), 0); }
END_TEST

// Test removing non-existent file with force
START_TEST(test_remove_nonexistent_file_force) {
    struct Options opts = default_opts;
    opts.force = true;

    ck_assert_int_eq(safe_remove("does_not_exist.txt", &opts), 0);
}
END_TEST

// Test dry-run mode
START_TEST(test_dry_run_mode) {
    const char *test_file = "dry_run_test.txt";
    create_test_file(test_file, "content");

    struct Options opts = default_opts;
    opts.dry_run = true;
    opts.verbose = true;

    // Test that dry run succeeds and doesn't actually remove the file
    ck_assert_int_eq(safe_remove(test_file, &opts), 0);

    // File should still exist in dry run mode
    ck_assert(file_exists(test_file));
}
END_TEST

// Test removing directory without recursive flag
START_TEST(test_remove_directory_non_recursive) {
    const char *test_dir_name = "test_directory";
    mkdir(test_dir_name, 0755);

    // Should fail without recursive flag
    ck_assert_int_ne(safe_remove(test_dir_name, &default_opts), 0);
    ck_assert(file_exists(test_dir_name));
}
END_TEST

// Test removing directory with recursive flag
START_TEST(test_remove_directory_recursive) {
    const char *test_dir_name = "test_directory";
    mkdir(test_dir_name, 0755);

    char file1[256], file2[256];
    snprintf(file1, sizeof(file1), "%s/file1.txt", test_dir_name);
    snprintf(file2, sizeof(file2), "%s/file2.txt", test_dir_name);
    create_test_file(file1, "content1");
    create_test_file(file2, "content2");

    struct Options opts = default_opts;
    opts.recursive = true;

    ck_assert_int_eq(safe_remove(test_dir_name, &opts), 0);
    ck_assert(!file_exists(test_dir_name));
}
END_TEST

// Test removing protected directory
START_TEST(test_remove_protected_directory) {
    struct Options opts = default_opts;
    opts.recursive = true;

    // Capture stderr
    int saved_stderr = dup(STDERR_FILENO);
    int pipe_fd[2];
    pipe(pipe_fd);
    dup2(pipe_fd[1], STDERR_FILENO);
    close(pipe_fd[1]);

    ck_assert_int_ne(safe_remove("/usr", &opts), 0);

    // Read captured error
    char buffer[256];
    int n = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    close(pipe_fd[0]);

    // Restore stderr
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    ck_assert_ptr_nonnull(strstr(buffer, "Protected system directory"));
}
END_TEST


// Test trash mode
START_TEST(test_remove_with_trash) {
    const char *test_file = "trash_test.txt";
    create_test_file(test_file, "content");

    struct Options opts = default_opts;
    opts.use_trash = true;
    opts.trash_dir = trash_dir;

    ck_assert_int_eq(safe_remove(test_file, &opts), 0);
    ck_assert(!file_exists(test_file));
}
END_TEST

// Test removing symlink
START_TEST(test_remove_symlink) {
    const char *target = "target.txt";
    const char *link = "test_link";

    create_test_file(target, "content");
    symlink(target, link);

    ck_assert_int_eq(safe_remove(link, &default_opts), 0);

    // Link should be gone
    ck_assert(!file_exists(link));

    // Target should still exist
    ck_assert(file_exists(target));
}
END_TEST

// Test removing nested directories
START_TEST(test_remove_nested_directories) {
    // Create nested structure
    mkdir("dir1", 0755);
    mkdir("dir1/dir2", 0755);
    mkdir("dir1/dir2/dir3", 0755);
    create_test_file("dir1/file1.txt", "content1");
    create_test_file("dir1/dir2/file2.txt", "content2");
    create_test_file("dir1/dir2/dir3/file3.txt", "content3");

    struct Options opts = default_opts;
    opts.recursive = true;

    ck_assert_int_eq(safe_remove("dir1", &opts), 0);
    ck_assert(!file_exists("dir1"));
}
END_TEST

// Create test suite
Suite *test_remove_operations_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Remove Operations");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_remove_file);
    tcase_add_test(tc_core, test_remove_nonexistent_file);
    tcase_add_test(tc_core, test_remove_nonexistent_file_force);
    tcase_add_test(tc_core, test_dry_run_mode);
    tcase_add_test(tc_core, test_remove_directory_non_recursive);
    tcase_add_test(tc_core, test_remove_directory_recursive);
    tcase_add_test(tc_core, test_remove_protected_directory);
    tcase_add_test(tc_core, test_remove_with_trash);
    tcase_add_test(tc_core, test_remove_symlink);
    tcase_add_test(tc_core, test_remove_nested_directories);

    suite_add_tcase(s, tc_core);

    return s;
}
