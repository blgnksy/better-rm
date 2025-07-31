#!/bin/bash
# install.sh - Installation script for better-rm

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Installation directories
PREFIX=${PREFIX:-/usr/local}
BINDIR=${BINDIR:-$PREFIX/bin}
SYSCONFDIR=${SYSCONFDIR:-/etc}
SYSTEMD_DIR=${SYSTEMD_DIR:-/etc/systemd/system}

echo -e "${GREEN}Installing better-rm...${NC}"

# Check if running as root for system-wide installation
if [[ $EUID -ne 0 ]] && [[ "$1" != "--user" ]]; then
   echo -e "${YELLOW}Warning: Not running as root. For system-wide installation, run with sudo.${NC}"
   echo "To install for current user only, run with --user flag"
   exit 1
fi

# Build the project
echo "Building better-rm..."
if command -v cmake &> /dev/null; then
    mkdir -p build
    cd build
    cmake ..
    make
    cd ..
    BINARY="build/better-rm"
else
    echo "CMake not found, trying direct compilation..."
    gcc -o better-rm src/main.c
    BINARY="better-rm"
fi

# User installation
if [[ "$1" == "--user" ]]; then
    echo -e "${GREEN}Performing user installation...${NC}"

    # Install binary to user's local bin
    mkdir -p "$HOME/.local/bin"
    cp "$BINARY" "$HOME/.local/bin/better-rm"
    chmod 755 "$HOME/.local/bin/better-rm"

    # Install user config
    if [[ ! -f "$HOME/.better-rm.conf" ]]; then
        cp config/better-rm.conf.example "$HOME/.better-rm.conf"
        echo -e "${GREEN}Created user config at $HOME/.better-rm.conf${NC}"
    else
        echo -e "${YELLOW}User config already exists at $HOME/.better-rm.conf${NC}"
    fi

    # Create user trash directory
    mkdir -p "$HOME/.Trash"

    echo -e "${GREEN}User installation complete!${NC}"
    echo "Add $HOME/.local/bin to your PATH if not already present:"
    echo "  echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc"
    echo ""
    echo "To use better-rm as default rm, add this alias to your ~/.bashrc:"
    echo "  alias rm='better-rm --trash'"

else
    # System-wide installation
    echo -e "${GREEN}Performing system-wide installation...${NC}"

    # Install binary
    install -m 755 "$BINARY" "$BINDIR/better-rm"
    echo "Installed binary to $BINDIR/better-rm"

    # Install system config
    if [[ ! -f "$SYSCONFDIR/better-rm.conf" ]]; then
        install -m 644 config/better-rm.conf.example "$SYSCONFDIR/better-rm.conf"
        echo "Created system config at $SYSCONFDIR/better-rm.conf"
    else
        echo -e "${YELLOW}System config already exists at $SYSCONFDIR/better-rm.conf${NC}"
        echo "Example config saved to $SYSCONFDIR/better-rm.conf.example"
        install -m 644 config/better-rm.conf.example "$SYSCONFDIR/better-rm.conf.example"
    fi

    # Install systemd units (if systemd is present)
    if command -v systemctl &> /dev/null; then
        echo "Installing systemd units..."

        # Install cleanup script
        install -m 755 systemd/trash-cleanup.sh "$BINDIR/trash-cleanup.sh"

        # Install systemd service and timer
        install -m 644 systemd/better-rm-trash-cleanup.service "$SYSTEMD_DIR/"
        install -m 644 systemd/better-rm-trash-cleanup.timer "$SYSTEMD_DIR/"

        # Reload systemd
        systemctl daemon-reload

        echo -e "${GREEN}Systemd units installed${NC}"
        echo "To enable automatic trash cleanup, run:"
        echo "  sudo systemctl enable --now better-rm-trash-cleanup.service"
        echo "  sudo systemctl enable --now better-rm-trash-cleanup.timer"
    fi

    echo -e "${GREEN}System-wide installation complete!${NC}"
    echo ""
    echo "To use better-rm as default rm system-wide, add to /etc/bash.bashrc:"
    echo "  alias rm='better-rm --trash'"
fi

echo ""
echo -e "${GREEN}Installation completed successfully!${NC}"
echo "Run 'better-rm --help' to see usage information"
