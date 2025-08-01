#include <check.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Function declarations from main.c
int move_to_trash(const char *path, const char *trash_dir, bool verbose);
int ensure_trash_dir(const char *trash_dir);
char *generate_trash_name(const char *original_path, const char *trash_dir);

// Test fixture data
static char *test_dir = NULL;
static char *trash_dir = NULL;

static void setup(void) {
    // Create test directory
    char temp_template[] = "/tmp/better_rm_trash_test_XXXXXX";
    test_dir = mkdtemp(temp_template);
    ck_assert_ptr_nonnull(test_dir);
    test_dir = strdup(test_dir);

    // Create trash directory
    trash_dir = malloc(strlen(test_dir) + 10);
    sprintf(trash_dir, "%s/.Trash", test_dir);
    ck_assert_int_eq(ensure_trash_dir(trash_dir), 0);

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

static char *find_in_trash(const char *original_name) {
    DIR *dir = opendir(trash_dir);
    if (!dir)
        return NULL;

    struct dirent *entry;
    static char found_path[256];
    found_path[0] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, original_name) == entry->d_name) {
            snprintf(found_path, sizeof(found_path), "%s/%s", trash_dir, entry->d_name);
            break;
        }
    }

    closedir(dir);
    return found_path[0] ? found_path : NULL;
}

// Test moving a file to trash
START_TEST(test_move_file_to_trash) {
    const char *test_file = "test_file.txt";
    create_test_file(test_file, "test content");

    // Move to trash
    ck_assert_int_eq(move_to_trash(test_file, trash_dir, false), 0);

    // Original file should be gone
    ck_assert(!file_exists(test_file));

    // File should be in trash
    char *trashed = find_in_trash("test_file.txt");
    ck_assert_ptr_nonnull(trashed);
    ck_assert(file_exists(trashed));
}
END_TEST

// Test moving file with verbose output
START_TEST(test_move_file_verbose) {
    const char *test_file = "verbose_test.txt";
    create_test_file(test_file, "content");

    // Simply test that the function succeeds with verbose=true
    // The actual verbose output testing can be done manually or with integration tests
    ck_assert_int_eq(move_to_trash(test_file, trash_dir, true), 0);

    // Verify file was moved
    ck_assert(!file_exists(test_file));
    char *trashed = find_in_trash("verbose_test.txt");
    ck_assert_ptr_nonnull(trashed);
}
END_TEST

// Test moving non-existent file
START_TEST(test_move_nonexistent_file) { ck_assert_int_ne(move_to_trash("does_not_exist.txt", trash_dir, false), 0); }
END_TEST

// Test moving directory to trash
START_TEST(test_move_directory_to_trash) {
    const char *test_dir_name = "test_directory";
    mkdir(test_dir_name, 0755);

    // Create a file inside
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/file_inside.txt", test_dir_name);
    create_test_file(file_path, "content");

    // Move directory to trash
    ck_assert_int_eq(move_to_trash(test_dir_name, trash_dir, false), 0);

    // Directory should be gone
    ck_assert(!file_exists(test_dir_name));

    // Directory should be in trash
    char *trashed = find_in_trash("test_directory");
    ck_assert_ptr_nonnull(trashed);
    ck_assert(file_exists(trashed));

    // File inside should still exist
    struct stat st;
    char inside_file[512];
    snprintf(inside_file, sizeof(inside_file), "%s/file_inside.txt", trashed);
    ck_assert_int_eq(stat(inside_file, &st), 0);
}
END_TEST

// Test moving symlink to trash
START_TEST(test_move_symlink_to_trash) {
    const char *target = "target_file.txt";
    const char *link = "test_symlink";

    create_test_file(target, "target content");
    ck_assert_int_eq(symlink(target, link), 0);

    // Move symlink to trash
    ck_assert_int_eq(move_to_trash(link, trash_dir, false), 0);

    // Symlink should be gone
    ck_assert(!file_exists(link));

    // Original target should still exist
    ck_assert(file_exists(target));

    // Symlink should be in trash
    char *trashed = find_in_trash("test_symlink");
    ck_assert_ptr_nonnull(trashed);
}
END_TEST

// Test trash name collision handling
START_TEST(test_trash_name_uniqueness) {
    const char *test_file = "duplicate.txt";

    // Move same filename multiple times
    for (int i = 0; i < 3; i++) {
        char content[32];
        snprintf(content, sizeof(content), "content %d", i);
        create_test_file(test_file, content);
        ck_assert_int_eq(move_to_trash(test_file, trash_dir, false), 0);
        sleep(1); // Ensure different timestamps
    }

    // Count files in trash with this name
    DIR *dir = opendir(trash_dir);
    ck_assert_ptr_nonnull(dir);

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "duplicate.txt") == entry->d_name) {
            count++;
        }
    }
    closedir(dir);

    ck_assert_int_eq(count, 3);
}
END_TEST

// Test moving file with special characters
START_TEST(test_move_file_special_chars) {
    const char *test_file = "file with spaces & special.txt";
    create_test_file(test_file, "content");

    ck_assert_int_eq(move_to_trash(test_file, trash_dir, false), 0);
    ck_assert(!file_exists(test_file));
}
END_TEST

// Test trash directory permissions
START_TEST(test_trash_dir_permissions) {
    struct stat st;
    ck_assert_int_eq(stat(trash_dir, &st), 0);

    // Should have 0700 permissions (owner only)
    ck_assert_int_eq(st.st_mode & 0777, 0700);
}
END_TEST

// Test moving read-only file
START_TEST(test_move_readonly_file) {
    const char *test_file = "readonly.txt";
    create_test_file(test_file, "content");

    // Make file read-only
    chmod(test_file, 0444);

    // Should still be able to move it
    ck_assert_int_eq(move_to_trash(test_file, trash_dir, false), 0);
    ck_assert(!file_exists(test_file));
}
END_TEST

// Create test suite
Suite *test_trash_operations_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Trash Operations");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_move_file_to_trash);
    tcase_add_test(tc_core, test_move_file_verbose);
    tcase_add_test(tc_core, test_move_nonexistent_file);
    tcase_add_test(tc_core, test_move_directory_to_trash);
    tcase_add_test(tc_core, test_move_symlink_to_trash);
    tcase_add_test(tc_core, test_trash_name_uniqueness);
    tcase_add_test(tc_core, test_move_file_special_chars);
    tcase_add_test(tc_core, test_trash_dir_permissions);
    tcase_add_test(tc_core, test_move_readonly_file);

    suite_add_tcase(s, tc_core);

    return s;
}
