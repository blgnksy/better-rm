[![Build and Test](https://github.com/blgnksy/better-rm/actions/workflows/build.yaml/badge.svg)](https://github.com/blgnksy/better-rm/actions/workflows/build.yaml)
[![publish_docs.yml](https://github.com/blgnksy/better-rm/actions/workflows/publish_docs.yml/badge.svg)](https://github.com/blgnksy/better-rm/actions/workflows/publish_docs.yml)
[![Pre-commit checks](https://github.com/blgnksy/better-rm/actions/workflows/pre-commit.yaml/badge.svg)](https://github.com/blgnksy/better-rm/actions/workflows/pre-commit.yaml)

# better-rm - A Better rm Command

A drop-in replacement for the `rm` command with protection against accidental deletion of system directories, trash/recovery functionality, and comprehensive logging.

## Features

- **Protected Directories**: Prevents deletion of critical system directories
- **Configuration Files**: System-wide and user-specific protected directory lists
- **Trash Mode**: Move files to trash instead of permanent deletion
- **Audit Logging**: All operations logged to syslog
- **Dry-Run Mode**: Test what would be deleted without actually removing
- **Additional Safety**: `--preserve-root`, `--one-file-system` options
- **Full Compatibility**: Supports standard `rm` options

## Installation

### Quick Development Setup
```bash
# Set up development environment with uv
./scripts/setup-dev.sh
```

### Manual Build
```bash
# Using CMake
mkdir build && cd build
cmake ..
make
sudo make install

# Using Make
make
sudo make install

# Or direct compilation
gcc -o better-rm src/main.c -I include -D_GNU_SOURCE
sudo cp better-rm /usr/local/bin/
```

### Cmake Targets

| Target                     | Purpose                                   |
| -------------------------- | ----------------------------------------- |
| `make`                     | Compile the binary                        |
| `make install`             | Install binary, config, systemd units     |
| `make uninstall`           | Remove installed files                    |
| `make install-user-config` | Setup per-user config and trash folder    |
| `make help`                | Print target help info                    |
| `make package`             | Generate `.tar.gz` package with CPack     |

## Usage

```bash
# Basic usage (same as rm)
better-rm file.txt
better-rm -rf directory/

# Move to trash instead of deleting
better-rm -t important.doc
better-rm --trash -r old_project/

# Dry-run to see what would be deleted
better-rm -n -r ~/Downloads/*

# View help
better-rm --help
```

### Recommended Aliases

Add to your `~/.bashrc`:
```bash
alias rm='better-rm --trash'    # Always use trash by default
alias rmf='better-rm'           # Force permanent deletion
alias rml='ls ~/.Trash'         # List trash contents
```

## Configuration

### System Configuration: `/etc/better-rm.conf`
```conf
# Protected directories
protect=/var/lib/mysql
protect=/var/lib/postgresql
protect=/opt/production
```

### User Configuration: `~/.better-rm.conf`
```conf
# Personal protected directories
protect=/home/user/important
protect=/home/user/work/production
```

### XDG Base Directory Support
The tool also checks for configuration in:
- `$XDG_CONFIG_HOME/better-rm/config`
- `~/.config/better-rm/config`

## Directory Structure

```
.
├── .github/workflows/      # GitHub Actions CI/CD
│   ├── build.yaml
│   ├── bump-version.yml
│   ├── pre-commit.yaml
│   ├── publish_docs.yml
│   └── release.yml
├── cmake/                  # CMake modules
│   └── cmake_uninstall.cmake.in
├── config/                 # Configuration examples
│   └── better-rm.conf.example
├── docs/                   # Documentation
│   ├── CMakeLists.txt
│   ├── COMMITIZEN_SETUP.md
│   ├── DEVELOPER_QUICK_REFERENCE.md
│   ├── Doxyfile.in
│   ├── GITHUB_WORKFLOWS.md
│   ├── TROUBLESHOOTING.md
│   ├── WORKFLOW_COMPARISON.md
│   └── html/              # Generated API docs
├── include/                # Header files
│   └── version.h
├── scripts/                # Utility scripts
│   ├── check-branch.sh
│   ├── debug-pre-commit.sh
│   ├── install.sh
│   ├── setup-dev.sh
│   └── test-ci-locally.sh
├── src/                    # Source code
│   └── main.c
├── systemd/                # Systemd integration
│   ├── better-rm-trash-cleanup.service
│   ├── better-rm-trash-cleanup.timer
│   └── trash-cleanup.sh
├── .clang-format          # Code formatting rules
├── .cz.toml              # Commitizen configuration
├── .gitignore            # Git ignore rules
├── .pre-commit-config.yaml # Pre-commit hooks
├── .python-version       # Python version for uv
├── CHANGELOG.md          # Auto-generated changelog
├── CMakeLists.txt        # CMake configuration
├── LICENCE.md            # MIT License
├── Makefile              # Alternative build system
├── pyproject.toml        # Python project config
├── README.md             # This file
├── uv.lock              # uv lock file
└── VERSION              # Version file
```

## Trash Management

### Manual Recovery
```bash
# List trash contents
ls -la ~/.Trash/

# Recover a file
mv ~/.Trash/document.txt.20240731_120000.1234 ~/document.txt
```

### Automatic Cleanup
Enable the systemd timer for automatic cleanup of old trash files:
```bash
sudo systemctl enable --now better-rm-trash-cleanup.timer
```

### Environment Variables
- `BETTER_RM_TRASH`: Override default trash directory
- `BETTER_RM_TRASH_DAYS`: Days to keep files in trash (default: 30)

## Logging

View deletion logs:
```bash
# Recent activity
sudo journalctl -t better-rm

# Follow in real-time
sudo journalctl -t better-rm -f
```

## Building from Source

### Requirements
- GCC or Clang compiler
- CMake 3.10+ (optional, for CMake build)
- Linux with glibc
- cppcheck, check and clang-format (for development)
- uv (for development with Commitizen)

### Install Build Dependencies
```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake cppcheck clang-format check clang

# Fedora/RHEL
sudo dnf install gcc cmake cppcheck clang-tools-extra check clang
```

### Development Setup
```bash
# Set up development environment
./scripts/setup-dev.sh

# Make commits using Commitizen
uv run cz commit
```

### Development Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Or using Make
make debug
```

### Create Distribution Package
```bash
mkdir build && cd build
cmake ..
make package  # Creates .deb, .rpm, and .tar.gz packages

# Or using Make
make dist
```

### Build Documentation
```bash
mkdir build && cd build
cmake ..
make docs  # Requires Doxygen
```

## Contributing

1. Fork the repository
2. Set up development environment: `./scripts/setup-dev.sh`
3. Create your feature branch (`git checkout -b feature/amazing-feature`)
4. Make commits using commitizen: `uv run cz commit`
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

### Development Workflow

The project uses GitHub Actions for CI/CD:

- **Pre-commit checks**: Runs on all PRs (formatting, linting)
- **Build and test**: Tests multiple compiler configurations
- **Version bumping**: Automatic versioning based on commits
- **Release creation**: Builds packages when tags are pushed

### Commit Convention

Follow conventional commits for automatic versioning:
- `feat:` - New features (minor version bump)
- `fix:` - Bug fixes (patch version bump)
- `feat!:` or `BREAKING CHANGE:` - Breaking changes (major version bump)

## License

This project is licensed under the GPL v2 - see the [LICENCE.md](LICENCE.md) file for details.

## Safety Notes

- Default protected directories include: `/`, `/bin`, `/boot`, `/dev`, `/etc`, `/home`, `/lib`, `/proc`, `/root`, `/sbin`, `/sys`, `/usr`, `/var`
- Always test with `--dry-run` when using wildcards or recursive deletion
- Consider using `--trash` mode by default through aliases
- Regular backups are still recommended

## Comparison with Standard rm

| Feature | rm | better-rm |
|---------|----|----|
| Delete files | ✓ | ✓ |
| Recursive deletion | ✓ | ✓ |
| Force mode | ✓ | ✓ |
| Protected directories | ✗ | ✓ |
| Trash/recovery | ✗ | ✓ |
| Audit logging | ✗ | ✓ |
| Dry-run mode | ✗ | ✓ |
| Config files | ✗ | ✓ |
| XDG Base Directory | ✗ | ✓ |


## TODO
- [ ] Support macOS (partial support, needs testing)
- [ ] Add shell completion scripts
- [ ] Create man page
