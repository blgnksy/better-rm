/*! \file main.c
 */
#define GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "../include/version.h"

// Default configuration
#define CONFIG_FILE "/etc/better-rm.conf"
#define USER_CONFIG_FILE ".better-rm.conf"
#define TRASH_DIR_ENV "BETTER_RM_TRASH"
#define DEFAULT_TRASH_DIR ".Trash"
#define MAX_PROTECTED_DIRS 100

const char *DEFAULT_PROTECTED_DIRS[] = {
        "/",      "/bin",  "/boot", "/dev",  "/etc", "/home", "/lib", "/lib32",
        "/lib64", "/proc", "/root", "/sbin", "/sys", "/usr",  "/var", NULL}; /*!< Built-in protected directories */

// Dynamic protected directories list
char *protected_dirs[MAX_PROTECTED_DIRS];
int protected_count = 0;

/*! Options struct used to store user defined options */
struct Options {
    bool recursive; /*!< remove directories and their contents recursively */
    bool force; /*!< ignore nonexistent files, never prompt */
    bool verbose; /*!< explain what is being done */
    bool interactive; /*!< prompt before every removal */
    bool dry_run; /*!< show what would be deleted without actually removing */
    bool preserve_root; /*!< block removing `/` */
    bool one_file_system; /*!< stay on the same filesystem */
    bool use_trash; /*!< move files to trash instead of deleting */
    bool no_preserve_root; /*!< allow removing `/` */
    char *trash_dir; /*!< specify trash directory */
};


/**
 * Initialize the default protected directories defined in \ref DEFAULT_PROTECTED_DIRS
 */
void init_protected_dirs() {
    int i = 0;
    while (DEFAULT_PROTECTED_DIRS[i] != NULL && protected_count < MAX_PROTECTED_DIRS) {
        protected_dirs[protected_count++] = strdup(DEFAULT_PROTECTED_DIRS[i]);
        i++;
    }
}


/** Loads the configuration for a given file
 *
 * @param filename absolute path of configuration file to be loaded
 */
void load_config_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return;

    char line[PATH_MAX];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
            continue;

        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        // Parse directives
        if (strncmp(line, "protect=", 8) == 0) {
            if (protected_count < MAX_PROTECTED_DIRS) {
                protected_dirs[protected_count++] = strdup(line + 8);
            }
        } else if (strncmp(line, "trash_dir=", 10) == 0) {
            // This would override the default trash directory
        }
    }

    fclose(fp);
}


/** Loads all config files
 *
 * First reads the global configuration file \ref CONFIG_FILE then resolves the user configuration
 * file location and loads to be able to update \ref DEFAULT_PROTECTED_DIRS
 */
void load_configs() {
    // System config
    load_config_file("/etc/better-rm.conf");

    // User config - check multiple locations
    char user_config[PATH_MAX];
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (config_home) {
        // Follow XDG Base Directory spec
        snprintf(user_config, sizeof(user_config), "%s/better-rm/config", config_home);
    } else if (home) {
        // Fallback to ~/.config
        snprintf(user_config, sizeof(user_config), "%s/.config/better-rm/config", home);
    }

    load_config_file(user_config);
}


/** Resolves the absolute path of trash directory
 *
 * @return trash directory's absolute path
 */
char *get_trash_dir() {
    // Check environment variable first
    char *env_trash = getenv(TRASH_DIR_ENV);
    if (env_trash)
        return env_trash;

    // Use default in home directory
    const char *home = getenv("HOME");
    if (home) {
        static char trash_path[PATH_MAX];
        snprintf(trash_path, sizeof(trash_path), "%s/%s", home, DEFAULT_TRASH_DIR);
        return trash_path;
    }

    return "/tmp/.Trash"; // Fallback
}


/** Create trash directory if it doesn't exist
 *
 * @param trash_dir trash directory absolute path
 * @return trash directory's absolute path
 */
int ensure_trash_dir(const char *trash_dir) {
    struct stat st;
    if (stat(trash_dir, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "better-rm: trash path exists but is not directory: %s\n", trash_dir);
            return -1;
        }
        return 0;
    }

    // Create directory with 0700 permissions
    if (mkdir(trash_dir, 0700) != 0) {
        fprintf(stderr, "better-rm: cannot create trash directory: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}


/** Generate unique filename while moving the file to the trash
 *
 * @param original_path file to be deleted(moved)
 * @param trash_dir trash directory
 * @return filename
 */
char *generate_trash_name(const char *original_path, const char *trash_dir) {
    static char trash_path[PATH_MAX];
    char *basename_copy = strdup(original_path);
    const char *base = basename(basename_copy);

    time_t now;
    time(&now);
    const struct tm *tm_info = localtime(&now);

    // Format: filename.YYYYMMDD_HHMMSS.pid
    snprintf(trash_path, sizeof(trash_path), "%s/%s.%04d%02d%02d_%02d%02d%02d.%d", trash_dir, base,
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min,
             tm_info->tm_sec, getpid());

    free(basename_copy);
    return trash_path;
}

/** Move file to the trash
 *
 * @param path absolute path of the file to be moved
 * @param trash_dir trash directory
 * @param verbose verbose output
 * @return 0 for success -1 for error
 */
int move_to_trash(const char *path, const char *trash_dir, bool verbose) {
    const char *trash_path = generate_trash_name(path, trash_dir);

    if (verbose) {
        printf("moving '%s' to trash as '%s'\n", path, trash_path);
    }

    if (rename(path, trash_path) != 0) {
        // If rename fails (different filesystem), try copy and delete
        fprintf(stderr, "better-rm: cannot move to trash: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/** Resolve path to its absolute form
 *
 * @param path path to be analyzed
 * @return absolute path
 */
char *resolve_path(const char *path) {
    char *resolved = realpath(path, NULL);
    if (resolved == NULL) {
        // If realpath fails, try to construct absolute path manually
        if (path[0] != '/') {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                resolved = malloc(PATH_MAX);
                if (resolved) {
                    snprintf(resolved, PATH_MAX, "%s/%s", cwd, path);
                }
            }
        } else {
            resolved = strdup(path);
        }
    }
    return resolved;
}

/** Function to check if path is protected
 *
 * @param path path to be analyzed
 * @return true if protected else false
 */
bool is_protected(const char *path) {
    char *resolved = resolve_path(path);
    if (resolved == NULL)
        return false;

    // Normalize path by removing trailing slashes
    size_t len = strlen(resolved);
    while (len > 1 && resolved[len - 1] == '/') {
        resolved[len - 1] = '\0';
        len--;
    }

    // Check against protected directories
    for (int i = 0; i < protected_count; i++) {
        if (strcmp(resolved, protected_dirs[i]) == 0) {
            free(resolved);
            return true;
        }
    }

    free(resolved);
    return false;
}

/** Check if path is root when preserve-root is enabled
 *
 * @param path path to be analyzed
 * @param opts provided options
 * @return true if `/` else false
 */
bool is_root_with_preserve(const char *path, const struct Options *opts) {
    if (!opts->preserve_root || opts->no_preserve_root)
        return false;

    char *resolved = resolve_path(path);
    if (resolved == NULL)
        return false;

    bool is_root = (strcmp(resolved, "/") == 0);
    free(resolved);
    return is_root;
}

/** Log deletion to syslog
 *
 * @param path deleted path
 * @param action TRASH or DELETE
 * @param success
 */
void log_deletion(const char *path, const char *action, bool success) {
    openlog("better-rm", LOG_PID, LOG_USER);

    if (success) {
        syslog(LOG_INFO, "%s: %s (user: %s, uid: %d)", action, path, getenv("USER"), getuid());
    } else {
        syslog(LOG_WARNING, "%s FAILED: %s (user: %s, uid: %d, error: %s)", action, path, getenv("USER"), getuid(),
               strerror(errno));
    }

    closelog();
}

/** Recursive directory removal
 *
 * @param path directory
 * @param opts provided options
 * @return 0 for success -1 for error
 */
int remove_directory(const char *path, const struct Options *opts) {
    DIR *dir = opendir(path);
    if (!dir) {
        return -1;
    }

    const struct dirent *entry;
    char full_path[PATH_MAX];
    int ret = 0;

    // Check if we should stay on the same filesystem
    dev_t dir_dev = 0;
    if (opts->one_file_system) {
        struct stat st;
        if (stat(path, &st) == 0) {
            dir_dev = st.st_dev;
        }
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == 0) {
            // Check filesystem boundary
            if (opts->one_file_system && dir_dev != 0 && st.st_dev != dir_dev) {
                if (opts->verbose) {
                    printf("skipping '%s': different filesystem\n", full_path);
                }
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                ret = remove_directory(full_path, opts);
            } else {
                if (opts->verbose || opts->dry_run) {
                    printf("%s%s '%s'\n", opts->dry_run ? "[DRY-RUN] would be " : "",
                           opts->use_trash ? "trashing" : "removing", full_path);
                }
                if (!opts->dry_run) {
                    if (opts->use_trash) {
                        ret = move_to_trash(full_path, opts->trash_dir, false);
                    } else {
                        ret = unlink(full_path);
                    }
                    log_deletion(full_path, opts->use_trash ? "TRASH" : "DELETE", ret == 0);
                }
            }

            if (ret != 0 && !opts->force) {
                break;
            }
        }
    }

    closedir(dir);

    if (ret == 0) {
        if (opts->verbose || opts->dry_run) {
            printf("%s%s directory '%s'\n", opts->dry_run ? "[DRY-RUN] would be " : "",
                   opts->use_trash ? "trashing" : "removing", path);
        }
        if (!opts->dry_run) {
            if (opts->use_trash) {
                ret = move_to_trash(path, opts->trash_dir, false);
            } else {
                ret = rmdir(path);
            }
            log_deletion(path, opts->use_trash ? "TRASH_DIR" : "DELETE_DIR", ret == 0);
        }
    }

    return ret;
}

/** Safe Remove
 *
 * Moves a fs object to trash if the path is not protected while preserving root directory
 *
 * @param path path to be removed
 * @param opts provided options
 * @return
 */
int safe_remove(const char *path, const struct Options *opts) {
    // Check if path is protected
    if (is_protected(path)) {
        fprintf(stderr, "%sbetter-rm: cannot remove '%s': Protected system directory\n",
                opts->dry_run ? "[DRY-RUN] " : "", path);
        return 1;
    }

    // Check preserve-root
    if (is_root_with_preserve(path, opts)) {
        fprintf(stderr, "%sbetter-rm: cannot remove '%s': --preserve-root is active\n",
                opts->dry_run ? "[DRY-RUN] " : "", path);
        return 1;
    }

    // Get file information
    struct stat st;
    if (lstat(path, &st) != 0) {
        if (!opts->force) {
            fprintf(stderr, "%sbetter-rm: cannot remove '%s': %s\n", opts->dry_run ? "[DRY-RUN] " : "", path,
                    strerror(errno));
            return 1;
        }
        return 0;
    }

    // Interactive mode
    if (opts->interactive && !opts->dry_run) {
        printf("remove '%s'? ", path);
        char response;
        if (scanf(" %c", &response) != 1 || (response != 'y' && response != 'Y')) {
            return 0;
        }
    }

    // Handle directories
    if (S_ISDIR(st.st_mode)) {
        if (!opts->recursive) {
            fprintf(stderr, "%sbetter-rm: cannot remove '%s': Is a directory\n", opts->dry_run ? "[DRY-RUN] " : "",
                    path);
            return 1;
        }

        if (opts->verbose || opts->dry_run) {
            printf("%s%s directory '%s' recursively\n", opts->dry_run ? "[DRY-RUN] would be " : "",
                   opts->use_trash ? "trashing" : "removing", path);
        }

        return remove_directory(path, opts) == 0 ? 0 : 1;
    } else {
        // Remove regular file or symlink
        if (opts->verbose || opts->dry_run) {
            printf("%s%s '%s'\n", opts->dry_run ? "[DRY-RUN] would be " : "", opts->use_trash ? "trashing" : "removing",
                   path);
        }

        if (!opts->dry_run) {
            int ret;
            if (opts->use_trash) {
                ret = move_to_trash(path, opts->trash_dir, false);
            } else {
                ret = unlink(path);
            }

            if (ret != 0) {
                if (!opts->force) {
                    fprintf(stderr, "better-rm: cannot %s '%s': %s\n", opts->use_trash ? "trash" : "remove", path,
                            strerror(errno));
                    return 1;
                }
            }

            log_deletion(path, opts->use_trash ? "TRASH" : "DELETE", ret == 0);
        }
    }

    return 0;
}

/** Print version information
 *
 */
void print_version() {
    printf("Version: %s\n", VERSION);
    printf("Copyright (C) 2025 Your Name\n");
    printf("License: MIT\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}


/** Print usage
 *
 * @param program_name binary name
 */
void print_usage(const char *program_name) {
    printf("Usage: %s [options] file...\n", program_name);
    printf("Better replacement for rm command with protection against deleting system directories\n\n");
    printf("Version %s\n\n", VERSION);
    printf("Options:\n");
    printf("  -r, -R, --recursive         remove directories and their contents recursively\n");
    printf("  -f, --force                 ignore nonexistent files, never prompt\n");
    printf("  -i                          prompt before every removal\n");
    printf("  -v, --verbose               explain what is being done\n");
    printf("  -n, --dry-run               show what would be deleted without actually removing\n");
    printf("  -t, --trash                 move files to trash instead of deleting\n");
    printf("      --trash-dir=DIR         specify trash directory (default: ~/%s)\n", DEFAULT_TRASH_DIR);
    printf("      --preserve-root         do not remove '/' (default)\n");
    printf("      --no-preserve-root      allow removing '/'\n");
    printf("      --one-file-system       stay on the same filesystem\n");
    printf("  -h, --help                  display this help and exit\n\n");
    printf("Environment variables:\n");
    printf("  BETTER_RM_TRASH             Override default trash directory\n\n");
    printf("Configuration files:\n");
    printf("  %s                    System-wide configuration\n", CONFIG_FILE);
    printf("  ~/%s                  User configuration\n\n", USER_CONFIG_FILE);
    printf("Protected directories: ");
    for (int i = 0; i < protected_count; i++) {
        printf("%s ", protected_dirs[i]);
    }
    printf("\n");
}

#ifndef UNIT_TESTING
int main(int argc, char *argv[]) {
    struct Options opts = {.recursive = false,
                           .force = false,
                           .verbose = false,
                           .interactive = false,
                           .dry_run = false,
                           .preserve_root = true, // Default to safe
                           .one_file_system = false,
                           .use_trash = false,
                           .no_preserve_root = false,
                           .trash_dir = NULL};

    // Initialize protected directories
    init_protected_dirs();
    load_configs();

    // Parse command line options
    int opt;
    static struct option long_options[] = {
            {"recursive", no_argument, 0, 'r'},     {"force", no_argument, 0, 'f'},
            {"verbose", no_argument, 0, 'v'},       {"dry-run", no_argument, 0, 'n'},
            {"trash", no_argument, 0, 't'},         {"trash-dir", required_argument, 0, 0},
            {"preserve-root", no_argument, 0, 0},   {"no-preserve-root", no_argument, 0, 0},
            {"one-file-system", no_argument, 0, 0}, {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'V'},       {0, 0, 0, 0}};

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "rRfivnthV", long_options, &option_index)) != -1) {
        switch (opt) {
            case 0:
                // Long options
                if (strcmp(long_options[option_index].name, "trash-dir") == 0) {
                    opts.trash_dir = optarg;
                    opts.use_trash = true;
                } else if (strcmp(long_options[option_index].name, "preserve-root") == 0) {
                    opts.preserve_root = true;
                    opts.no_preserve_root = false;
                } else if (strcmp(long_options[option_index].name, "no-preserve-root") == 0) {
                    opts.no_preserve_root = true;
                    opts.preserve_root = false;
                } else if (strcmp(long_options[option_index].name, "one-file-system") == 0) {
                    opts.one_file_system = true;
                }
                break;
            case 'r':
            case 'R':
                opts.recursive = true;
                break;
            case 'f':
                opts.force = true;
                opts.interactive = false;
                break;
            case 'i':
                opts.interactive = true;
                opts.force = false;
                break;
            case 'v':
                opts.verbose = true;
                break;
            case 'n':
                opts.dry_run = true;
                opts.verbose = true; // Dry-run implies verbose
                break;
            case 't':
                opts.use_trash = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'V':
                print_version();
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check if any files were specified
    if (optind >= argc) {
        fprintf(stderr, "better-rm: missing operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // Setup trash directory if needed
    if (opts.use_trash && !opts.trash_dir) {
        opts.trash_dir = get_trash_dir();
    }

    if (opts.use_trash && !opts.dry_run) {
        if (ensure_trash_dir(opts.trash_dir) != 0) {
            return 1;
        }
    }

    // Show dry-run header if enabled
    if (opts.dry_run) {
        printf("=== DRY-RUN MODE: No files will be actually deleted ===\n");
        if (opts.use_trash) {
            printf("=== TRASH MODE: Files would be moved to %s ===\n", opts.trash_dir);
        }
    }

    // Process each file
    int exit_status = 0;
    for (int i = optind; i < argc; i++) {
        if (safe_remove(argv[i], &opts) != 0) {
            exit_status = 1;
        }
    }

    // Show dry-run footer if enabled
    if (opts.dry_run) {
        printf("=== DRY-RUN COMPLETE: No files were actually deleted ===\n");
    }

    // Cleanup
    for (int i = 0; i < protected_count; i++) {
        free(protected_dirs[i]);
    }

    return exit_status;
}
#endif // UNIT_TESTING
