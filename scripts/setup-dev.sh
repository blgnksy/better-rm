#!/bin/bash
# setup-dev.sh - Set up development environment for better-rm
#
# Usage: ./scripts/setup-dev.sh

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}Setting up better-rm development environment...${NC}"

# Check for required system tools
echo -e "${GREEN}Checking system dependencies...${NC}"
missing_tools=""

for tool in gcc cmake cppcheck clang-format clang check; do
    if ! command -v $tool &> /dev/null; then
        missing_tools="$missing_tools $tool"
    fi
done

if [ -n "$missing_tools" ]; then
    echo -e "${YELLOW}Missing tools:$missing_tools${NC}"
    echo "Please install them using your package manager:"
    echo ""
    echo "  # Debian/Ubuntu:"
    echo "  sudo apt-get install build-essential cmake cppcheck clang-format check"
    echo ""
    echo "  # Fedora:"
    echo "  sudo dnf install gcc cmake cppcheck clang-tools-extra check"
    echo ""
    exit 1
fi

# Check if uv is installed
if ! command -v uv &> /dev/null; then
    echo -e "${YELLOW}uv not found. Installing uv...${NC}"

    # Detect OS
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        echo "Windows is/will not be supported!"
        exit 1
    else
        curl -LsSf https://astral.sh/uv/install.sh | sh
    fi
fi

echo -e "${GREEN}Creating Python 3.8 virtual environment...${NC}"
uv venv --python 3.8

echo -e "${GREEN}Installing development dependencies...${NC}"
uv pip install -e ".[dev]"

echo -e "${GREEN}Setting up pre-commit hooks...${NC}"
uv run pre-commit install
uv run pre-commit install --hook-type commit-msg

echo -e "${GREEN}Development environment setup complete!${NC}"
echo ""
echo "To make commits, use:"
echo "  uv run cz commit"
echo ""
echo "Or commit directly with conventional format:"
echo "  git commit -m \"feat: your feature description\""
echo ""
echo "To bump version manually:"
echo "  uv run cz bump --changelog"
