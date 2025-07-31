#!/bin/bash
#
# trash-cleanup.sh - Clean old files from better-rm trash directories
# This script is run by systemd timer to automatically clean trash files
# older than a specified number of days.

set -euo pipefail

# Configuration
DAYS_TO_KEEP=${BETTER_RM_TRASH_DAYS:-30}  # Default: keep files for 30 days
LOG_TAG="better-rm-trash-cleanup"

# Logging function
log() {
    logger -t "$LOG_TAG" "$@"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $*"
}

# Error handler
error_exit() {
    log "ERROR: $1"
    exit 1
}

# Validate environment
if ! command -v find >/dev/null 2>&1; then
    error_exit "find command not found"
fi

if ! command -v logger >/dev/null 2>&1; then
    # If logger is not available, just use echo
    logger() { shift; echo "$@"; }
fi

log "Starting trash cleanup (keeping files newer than $DAYS_TO_KEEP days)"

# Track statistics
total_files=0
total_dirs=0
total_size=0
errors=0

# Function to get size of file in bytes (portable)
get_size() {
    if stat --version >/dev/null 2>&1; then
        # GNU stat
        stat -c %s "$1" 2>/dev/null || echo 0
    else
        # BSD stat
        stat -f %z "$1" 2>/dev/null || echo 0
    fi
}

# Function to clean a trash directory
clean_trash_dir() {
    local trash_dir="$1"
    local user="$2"
    local files_removed=0
    local dirs_removed=0
    local space_freed=0

    if [[ ! -d "$trash_dir" ]]; then
        log "Trash directory does not exist: $trash_dir (skipping)"
        return 0
    fi

    log "Cleaning trash directory: $trash_dir (user: $user)"

    # Find and remove old files
    while IFS= read -r -d '' file; do
        if [[ -f "$file" || -L "$file" ]]; then
            size=$(get_size "$file")
            if rm -f "$file" 2>/dev/null; then
                ((files_removed++))
                ((space_freed+=size))
                ((total_files++))
                ((total_size+=size))
            else
                log "WARNING: Failed to remove file: $file"
                ((errors++))
            fi
        fi
    done < <(find "$trash_dir" -type f -mtime +"$DAYS_TO_KEEP" -print0 2>/dev/null)

    # Find and remove old symlinks
    while IFS= read -r -d '' link; do
        if rm -f "$link" 2>/dev/null; then
            ((files_removed++))
            ((total_files++))
        else
            log "WARNING: Failed to remove symlink: $link"
            ((errors++))
        fi
    done < <(find "$trash_dir" -type l -mtime +"$DAYS_TO_KEEP" -print0 2>/dev/null)

    # Remove empty directories (bottom-up)
    while IFS= read -r dir; do
        if rmdir "$dir" 2>/dev/null; then
            ((dirs_removed++))
            ((total_dirs++))
        fi
    done < <(find "$trash_dir" -type d -empty -print 2>/dev/null | sort -r)

    # Convert bytes to human readable
    if ((space_freed > 1073741824)); then
        space_freed_human="$((space_freed / 1073741824))GB"
    elif ((space_freed > 1048576)); then
        space_freed_human="$((space_freed / 1048576))MB"
    elif ((space_freed > 1024)); then
        space_freed_human="$((space_freed / 1024))KB"
    else
        space_freed_human="${space_freed}B"
    fi

    if ((files_removed > 0 || dirs_removed > 0)); then
        log "Cleaned $trash_dir: removed $files_removed files, $dirs_removed directories, freed $space_freed_human"
    fi
}

# Clean system-wide trash locations
clean_trash_dir "/tmp/.Trash" "system"

# Clean root trash
if [[ -d "/root/.Trash" ]]; then
    clean_trash_dir "/root/.Trash" "root"
fi

# Clean user trash directories
for home_dir in /home/*; do
    if [[ -d "$home_dir" ]]; then
        username=$(basename "$home_dir")
        trash_dir="$home_dir/.Trash"

        # Also check for environment variable override
        user_trash_env="$home_dir/.config/better-rm/trash_dir"
        if [[ -f "$user_trash_env" ]]; then
            custom_trash=$(cat "$user_trash_env" 2>/dev/null)
            if [[ -n "$custom_trash" && -d "$custom_trash" ]]; then
                clean_trash_dir "$custom_trash" "$username"
                continue
            fi
        fi

        clean_trash_dir "$trash_dir" "$username"
    fi
done

# Summary
if ((total_size > 1073741824)); then
    total_size_human="$((total_size / 1073741824))GB"
elif ((total_size > 1048576)); then
    total_size_human="$((total_size / 1048576))MB"
elif ((total_size > 1024)); then
    total_size_human="$((total_size / 1024))KB"
else
    total_size_human="${total_size}B"
fi

log "Cleanup complete: removed $total_files files and $total_dirs directories, freed $total_size_human total"

if ((errors > 0)); then
    log "WARNING: Encountered $errors errors during cleanup"
    exit 1
fi

exit 0