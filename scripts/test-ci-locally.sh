#!/bin/bash
# test-ci-locally.sh - Test CI checks locally before pushing
#
# This script mimics what happens in GitHub Actions

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Running CI checks locally...${NC}"

# Check system dependencies
echo -e "\n${YELLOW}Checking system dependencies...${NC}"
for tool in gcc cmake cppcheck clang-format; do
    if command -v $tool &> /dev/null; then
        echo "✓ $tool: $(command -v $tool)"
    else
        echo -e "${RED}✗ $tool: NOT FOUND${NC}"
        exit 1
    fi
done

# Run pre-commit checks
echo -e "\n${YELLOW}Running pre-commit hooks...${NC}"
if command -v uv &> /dev/null; then
    uv run pre-commit run --all-files --show-diff-on-failure || {
        echo -e "${RED}Pre-commit checks failed!${NC}"
        echo "Fix the issues above and try again."
        exit 1
    }
else
    echo -e "${RED}uv not found. Please run ./scripts/setup-dev.sh first${NC}"
    exit 1
fi

# Test build with different configurations
echo -e "\n${YELLOW}Testing builds...${NC}"

# Clean previous builds
rm -rf build-*

# Test Debug build with GCC
echo -e "\n${YELLOW}Building with GCC (Debug)...${NC}"
cmake -B build-gcc-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc
cmake --build build-gcc-debug

# Test Release build with GCC
echo -e "\n${YELLOW}Building with GCC (Release)...${NC}"
cmake -B build-gcc-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
cmake --build build-gcc-release

# Test with Clang if available
if command -v clang &> /dev/null; then
    echo -e "\n${YELLOW}Building with Clang (Debug)...${NC}"
    cmake -B build-clang-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
    cmake --build build-clang-debug
fi

# Run cppcheck
echo -e "\n${YELLOW}Running cppcheck...${NC}"
cppcheck --enable=all --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --inline-suppr \
    -I include/ \
    src/

# Test the binary
echo -e "\n${YELLOW}Testing binary...${NC}"
./build-gcc-release/safe_rm --version
./build-gcc-release/safe_rm --help

# Check commit message format (if in git repo)
if git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "\n${YELLOW}Checking recent commit messages...${NC}"

    # Get last commit message
    last_commit_msg=$(git log -1 --pretty=%B)
    echo "Last commit: $last_commit_msg"

    # Validate it
    echo "$last_commit_msg" | uv run cz check --commit-msg-file - || {
        echo -e "${YELLOW}Warning: Last commit doesn't follow conventional format${NC}"
        echo "Next commit should use format like: feat:, fix:, docs:, etc."
    }
fi

echo -e "\n${GREEN}All CI checks passed! ✨${NC}"
echo "Your code is ready to push."

# Cleanup
echo -e "\n${YELLOW}Cleaning up test builds...${NC}"
rm -rf build-*

echo -e "${GREEN}Done!${NC}"
