/*! \file main.c
 */
#define GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
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
    const char *trash_dir; /*!< specify trash directory */
};

// Forward declarations
int remove_directory(const char *path, const struct Options *opts);
int move_to_trash(const char *path, const char *trash_dir, bool verbose);
char *generate_trash_name(const char *original_path, const char *trash_dir);


/**
 * Initialize the default protected directories defined in \ref DEFAULT_PROTECTED_DIRS
 */
void init_protected_dirs(void) {
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

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            continue;

        // Remove newline
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
            line_len--;
        }
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        // Parse directives
        if (strncmp(line, "protect=", 8) == 0) {
            if (protected_count < MAX_PROTECTED_DIRS) {
                protected_dirs[protected_count++] = strdup(line + 8);
            } else {
                fprintf(stderr, "better-rm: warning: maximum number of protected directories reached (%d)\n",
                        MAX_PROTECTED_DIRS);
            }
        } else if (strncmp(line, "trash_dir=", 10) == 0) {
            // This would override the default trash directory
        }
    }

    free(line);
    fclose(fp);
}


/** Loads all config files
 *
 * First reads the global configuration file \ref CONFIG_FILE then resolves the user configuration
 * file location and loads to be able to update \ref DEFAULT_PROTECTED_DIRS
 */
void load_configs(void) {
    const char *home = getenv("HOME");
    bool user_config_loaded = false;

    if (home) {
        char user_config[PATH_MAX];
        const char *config_home = getenv("XDG_CONFIG_HOME");

        // 1. Try XDG config path
        if (config_home && config_home[0] != '\0') {
            snprintf(user_config, sizeof(user_config), "%s/better-rm/config", config_home);
        } else {
            snprintf(user_config, sizeof(user_config), "%s/.config/better-rm/config", home);
        }

        if (access(user_config, R_OK) == 0) {
            load_config_file(user_config);
            user_config_loaded = true;
        }

        // 2. Try legacy dot-file if XDG was not found
        if (!user_config_loaded) {
            snprintf(user_config, sizeof(user_config), "%s/%s", home, USER_CONFIG_FILE);
            if (access(user_config, R_OK) == 0) {
                load_config_file(user_config);
                user_config_loaded = true;
            }
        }
    }

    // 3. Load global config ONLY if no user config was loaded
    if (!user_config_loaded) {
        if (access(CONFIG_FILE, R_OK) == 0) {
            load_config_file(CONFIG_FILE);
        }
    }
}


/** Resolves the absolute path of trash directory
 *
 * @return trash directory's absolute path
 */
const char *get_trash_dir(void) {
    // Check environment variable first
    char *env_trash = getenv(TRASH_DIR_ENV);
    if (env_trash)
        return env_trash;

    // Use default in home directory
    const char *home = getenv("HOME");
    if (home) {
        static char *trash_path = NULL;
        if (trash_path)
            free(trash_path);
        if (asprintf(&trash_path, "%s/%s", home, DEFAULT_TRASH_DIR) == -1)
            return "/tmp/.Trash";
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
    char *basename_copy = strdup(original_path);
    if (!basename_copy)
        return NULL;
    const char *base = basename(basename_copy);

    time_t now;
    time(&now);
    const struct tm *tm_info = localtime(&now);

    char *trash_path = NULL;
    // Format: filename.YYYYMMDD_HHMMSS.pid
    if (asprintf(&trash_path, "%s/%s.%04d%02d%02d_%02d%02d%02d.%d", trash_dir, base, tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                 getpid()) == -1) {
        free(basename_copy);
        return NULL;
    }

    free(basename_copy);
    return trash_path;
}

/** Copy a single file
 *
 * @param src source path
 * @param dst destination path
 * @return 0 for success -1 for error
 */
int copy_file(const char *src, const char *dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
        return -1;

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        close(src_fd);
        return -1;
    }

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        close(src_fd);
        return -1;
    }

    char buf[16384];
    ssize_t nread;
    while ((nread = read(src_fd, buf, sizeof(buf))) > 0) {
        if (write(dst_fd, buf, nread) != nread) {
            close(src_fd);
            close(dst_fd);
            return -1;
        }
    }

    close(src_fd);
    close(dst_fd);
    return (nread < 0) ? -1 : 0;
}

/** Recursively copy a file or directory
 *
 * @param src source path
 * @param dst destination path
 * @return 0 for success -1 for error
 */
int copy_recursive(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) < 0)
        return -1;

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode) < 0 && errno != EEXIST)
            return -1;

        DIR *dir = opendir(src);
        if (!dir)
            return -1;

        struct dirent *entry;
        int ret = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char *src_path = NULL, *dst_path = NULL;
            if (asprintf(&src_path, "%s/%s", src, entry->d_name) == -1 ||
                asprintf(&dst_path, "%s/%s", dst, entry->d_name) == -1) {
                free(src_path);
                free(dst_path);
                ret = -1;
                break;
            }

            ret = copy_recursive(src_path, dst_path);
            free(src_path);
            free(dst_path);
            if (ret < 0)
                break;
        }
        closedir(dir);
        return ret;
    } else if (S_ISLNK(st.st_mode)) {
        char link_target[PATH_MAX];
        ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
        if (len < 0)
            return -1;
        link_target[len] = '\0';
        return symlink(link_target, dst);
    } else {
        return copy_file(src, dst);
    }
}

/** Move file to the trash
 *
 * @param path absolute path of the file to be moved
 * @param trash_dir trash directory
 * @param verbose verbose output
 * @return 0 for success -1 for error
 */
int move_to_trash(const char *path, const char *trash_dir, bool verbose) {
    char *trash_path = generate_trash_name(path, trash_dir);
    if (!trash_path)
        return -1;

    if (verbose) {
        printf("moving '%s' to trash as '%s'\n", path, trash_path);
    }

    if (rename(path, trash_path) != 0) {
        if (errno == EXDEV) {
            // Cross-filesystem move: copy to temp and then rename
            char *trash_tmp = NULL;
            if (asprintf(&trash_tmp, "%s.tmp.%d", trash_path, getpid()) == -1) {
                free(trash_path);
                return -1;
            }

            if (copy_recursive(path, trash_tmp) == 0) {
                // Atomically rename the temp copy to the final name
                if (rename(trash_tmp, trash_path) == 0) {
                    // Success! Now remove the original
                    // We need a way to call remove_directory/unlink without trashing
                    struct Options silent_opts = {.use_trash = false, .recursive = true, .force = true};
                    struct stat st;
                    if (lstat(path, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            remove_directory(path, &silent_opts);
                        } else {
                            unlink(path);
                        }
                    }
                    free(trash_tmp);
                    free(trash_path);
                    return 0;
                }
            }
            // Cleanup temp if it exists
            struct Options cleanup_opts = {.use_trash = false, .recursive = true, .force = true};
            remove_directory(trash_tmp, &cleanup_opts);
            free(trash_tmp);
        }
        fprintf(stderr, "better-rm: cannot move to trash: %s\n", strerror(errno));
        free(trash_path);
        return -1;
    }

    free(trash_path);
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
                    snprintf(resolved, PATH_MAX + 1, "%s/%s", cwd, path);
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

    // Normalize path by removing trailing slashes (except for "/")
    size_t len = strlen(resolved);
    while (len > 1 && resolved[len - 1] == '/') {
        resolved[len - 1] = '\0';
        len--;
    }

    // Check against protected directories
    for (int i = 0; i < protected_count; i++) {
        // Also normalize the protected directory entry for comparison
        char *prot_dir = strdup(protected_dirs[i]);
        if (prot_dir) {
            size_t p_len = strlen(prot_dir);
            while (p_len > 1 && prot_dir[p_len - 1] == '/') {
                prot_dir[p_len - 1] = '\0';
                p_len--;
            }
            if (strcmp(resolved, prot_dir) == 0) {
                free(prot_dir);
                free(resolved);
                return true;
            }
            free(prot_dir);
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

    const char *user = getenv("USER");
    if (!user) {
        struct passwd *pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "unknown";
    }

    if (success) {
        syslog(LOG_INFO, "%s: %s (user: %s, uid: %u)", action, path, user, getuid());
    } else {
        syslog(LOG_WARNING, "%s FAILED: %s (user: %s, uid: %u, error: %s)", action, path, user, getuid(),
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
    // If using trash, we can try to move the whole directory at once
    if (opts->use_trash) {
        if (opts->verbose || opts->dry_run) {
            printf("%strashing directory '%s'\n", opts->dry_run ? "[DRY-RUN] " : "", path);
        }
        if (!opts->dry_run) {
            int ret = move_to_trash(path, opts->trash_dir, false);
            log_deletion(path, "TRASH_DIR", ret == 0);
            return ret;
        }
        return 0;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        return -1;
    }

    const struct dirent *entry;
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

        char *full_path = NULL;
        if (asprintf(&full_path, "%s/%s", path, entry->d_name) == -1) {
            ret = -1;
            break;
        }

        struct stat st;
        if (lstat(full_path, &st) == 0) {
            // Check filesystem boundary
            if (opts->one_file_system && dir_dev != 0 && st.st_dev != dir_dev) {
                if (opts->verbose) {
                    printf("skipping '%s': different filesystem\n", full_path);
                }
                free(full_path);
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                ret = remove_directory(full_path, opts);
            } else {
                if (opts->verbose || opts->dry_run) {
                    printf("%sremoving '%s'\n", opts->dry_run ? "[DRY-RUN] " : "", full_path);
                }
                if (!opts->dry_run) {
                    ret = unlink(full_path);
                    log_deletion(full_path, "DELETE", ret == 0);
                }
            }

            if (ret != 0 && !opts->force) {
                free(full_path);
                break;
            }
        }
        free(full_path);
    }

    closedir(dir);

    if (ret == 0) {
        if (opts->verbose || opts->dry_run) {
            printf("%sremoving directory '%s'\n", opts->dry_run ? "[DRY-RUN] " : "", path);
        }
        if (!opts->dry_run) {
            ret = rmdir(path);
            log_deletion(path, "DELETE_DIR", ret == 0);
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
void print_version(void) {
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
    printf("Configuration files (User config overwrites System config):\n");
    printf("  ~/.config/better-rm/config  Modern user configuration (XDG)\n");
    printf("  ~/%-25s Legacy user configuration\n", USER_CONFIG_FILE);
    printf("  %-27s System-wide configuration\n\n", CONFIG_FILE);
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
                opts.verbose = true;
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

    // Note: static trash_path in get_trash_dir or long-lived opts.trash_dir
    // might need careful freeing if we wanted to be 100% leak-free on exit,
    // but for this utility, exiting is sufficient.

    return exit_status;
}
#endif // UNIT_TESTING
