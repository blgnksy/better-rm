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

### Quick Install (System-wide)
```bash
sudo ./scripts/installation.sh
```

### User-only Install
```bash
./scripts/install.sh --user
```

### Manual Build
```bash
# Using CMake
mkdir build && cd build
cmake ..
make
sudo make install

# Or direct compilation
gcc -o better-rm src/main.c
sudo cp better-rm /usr/local/bin/
```

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
alias rml='ls ~/.Trash'       # List trash contents
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

## Directory Structure

```
.
├── src/                    # Source code
│   └── main.c
├── config/                 # Configuration examples
│   └── better-rm.conf.example
├── systemd/                # Systemd service files
│   ├── better-rm-trash-cleanup.service
│   ├── better-rm-trash-cleanup.timer
│   └── trash-cleanup.sh
├── scripts/                # Installation scripts
│   └── install.sh
├── CMakeLists.txt         # CMake build configuration
└── README.md              # This file
```

## Trash Management

### Manual Recovery
```bash
# List trash contents
ls -la ~/.Trash/

# Recover better_rm-trash-cleanup.service file
mv ~/.Trash/document.txt.20240731_120000.1234 ~/document.txt
```

### Automatic Cleanup
Enable the systemd timer for automatic cleanup of old trash files:
```bash
sudo systemctl enable --now better-rm-trash-cleanup.timer
```

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

### Development Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Create Distribution Package
```bash
mkdir build && cd build
cmake ..
make package  # Creates .deb, .rpm, and .tar.gz packages
```

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

[Your chosen license]

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
