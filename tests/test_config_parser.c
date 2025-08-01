#include <check.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Function declarations from main.c
void load_config_file(const char *filename);
void load_configs(void);
bool is_protected(const char *path);

// Access to protected dirs for testing
extern char *protected_dirs[];
extern int protected_count;

// Test fixture data
static char *test_dir = NULL;
static char *home_backup = NULL;
static int initial_protected_count;

static void setup(void) {
    // Save initial state
    initial_protected_count = protected_count;

    // Backup HOME
    const char *home = getenv("HOME");
    if (home) {
        home_backup = strdup(home);
    }

    // Create test directory
    char temp_template[] = "/tmp/better_rm_config_test_XXXXXX";
    test_dir = mkdtemp(temp_template);
    ck_assert_ptr_nonnull(test_dir);
    test_dir = strdup(test_dir);

    // Set HOME to test directory
    setenv("HOME", test_dir, 1);
}

static void teardown(void) {
    // Restore HOME
    if (home_backup) {
        setenv("HOME", home_backup, 1);
        free(home_backup);
        home_backup = NULL;
    }

    // Clean up test directory
    if (test_dir) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
        system(cmd);
        free(test_dir);
        test_dir = NULL;
    }

    // Clean up protected dirs added during tests
    for (int i = initial_protected_count; i < protected_count; i++) {
        free(protected_dirs[i]);
    }
    protected_count = initial_protected_count;
}

static void write_config(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    ck_assert_ptr_nonnull(file);
    fprintf(file, "%s", content);
    fclose(file);
}

// Test empty config file
START_TEST(test_empty_config) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/empty.conf", test_dir);
    write_config(config_path, "");

    int count_before = protected_count;
    load_config_file(config_path);

    ck_assert_int_eq(protected_count, count_before);
}
END_TEST

// Test config with only comments
START_TEST(test_comments_only) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/comments.conf", test_dir);
    write_config(config_path, "# This is a comment\n"
                              "# Another comment\n"
                              "## Yet another comment\n"
                              "   # Indented comment\n");

    int count_before = protected_count;
    load_config_file(config_path);

    ck_assert_int_eq(protected_count, count_before);
}
END_TEST

// Test valid protect directives
START_TEST(test_valid_protect_directives) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/valid.conf", test_dir);
    write_config(config_path, "protect=/test/dir1\n"
                              "protect=/test/dir2\n"
                              "protect=/opt/custom\n");

    int count_before = protected_count;
    load_config_file(config_path);

    ck_assert_int_eq(protected_count, count_before + 3);
    ck_assert(is_protected("/test/dir1"));
    ck_assert(is_protected("/test/dir2"));
    ck_assert(is_protected("/opt/custom"));
}
END_TEST


// Test protect directive with spaces
START_TEST(test_protect_with_spaces) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/spaces.conf", test_dir);
    write_config(config_path,
                 "protect = /with/spaces\n" // This should NOT work
                 "protect=/no/spaces\n");

    int count_before = protected_count;
    load_config_file(config_path);

    // Only the one without spaces should work
    ck_assert_int_eq(protected_count, count_before + 1);
    ck_assert(!is_protected("/with/spaces"));
    ck_assert(is_protected("/no/spaces"));
}
END_TEST


// Test XDG config directory
START_TEST(test_xdg_config_dir) {
    // Create XDG config structure
    char xdg_config[256], better_rm_config[256], config_path[256];
    snprintf(xdg_config, sizeof(xdg_config), "%s/.config", test_dir);
    snprintf(better_rm_config, sizeof(better_rm_config), "%s/better-rm", xdg_config);

    mkdir(xdg_config, 0755);
    mkdir(better_rm_config, 0755);

    snprintf(config_path, sizeof(config_path), "%s/config", better_rm_config);
    write_config(config_path, "protect=/xdg/test1\n"
                              "protect=/xdg/test2\n");

    int count_before = protected_count;
    load_configs();

    // Should load XDG config
    ck_assert(protected_count >= count_before + 2);
    ck_assert(is_protected("/xdg/test1"));
    ck_assert(is_protected("/xdg/test2"));
}
END_TEST

// Test XDG_CONFIG_HOME environment variable
START_TEST(test_xdg_config_home_env) {
    char xdg_home[256], better_rm_config[256], config_path[256];
    snprintf(xdg_home, sizeof(xdg_home), "%s/custom_config", test_dir);
    snprintf(better_rm_config, sizeof(better_rm_config), "%s/better-rm", xdg_home);

    mkdir(xdg_home, 0755);
    mkdir(better_rm_config, 0755);

    setenv("XDG_CONFIG_HOME", xdg_home, 1);

    snprintf(config_path, sizeof(config_path), "%s/config", better_rm_config);
    write_config(config_path, "protect=/xdg/custom1\n"
                              "protect=/xdg/custom2\n");

    int count_before = protected_count;
    load_configs();

    ck_assert(protected_count >= count_before + 2);
    ck_assert(is_protected("/xdg/custom1"));
    ck_assert(is_protected("/xdg/custom2"));

    unsetenv("XDG_CONFIG_HOME");
}
END_TEST

// Test very long lines
START_TEST(test_long_lines) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/long.conf", test_dir);

    // Create a very long path (but still valid)
    char long_path[1024] = "protect=/very";
    for (int i = 0; i < 50; i++) {
        strcat(long_path, "/long");
    }
    strcat(long_path, "/path\n");
    strcat(long_path, "protect=/short\n");

    write_config(config_path, long_path);

    int count_before = protected_count;
    load_config_file(config_path);

    // Both should be loaded
    ck_assert_int_eq(protected_count, count_before + 2);
    ck_assert(is_protected("/short"));
}
END_TEST


// Create test suite
Suite *test_config_parser_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Config Parser");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_empty_config);
    tcase_add_test(tc_core, test_comments_only);
    tcase_add_test(tc_core, test_valid_protect_directives);
    tcase_add_test(tc_core, test_protect_with_spaces);
    tcase_add_test(tc_core, test_xdg_config_dir);
    tcase_add_test(tc_core, test_xdg_config_home_env);
    tcase_add_test(tc_core, test_long_lines);

    suite_add_tcase(s, tc_core);

    return s;
}
