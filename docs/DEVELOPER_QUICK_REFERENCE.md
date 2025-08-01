# Developer Quick Reference

## First Time Setup
```bash
# Clone and setup
git clone git@github.com:blgnksy/better-rm.git
cd better-rm
./scripts/setup-dev.sh
```

## Daily Development

### Making Changes
```bash
# Create feature branch
git checkout -b feat/my-feature

# Make changes to code
vim src/main.c

# Format code
uv run pre-commit run clang-format

# Test build
mkdir build && cd build
cmake .. && make
./better-rm --version
cd ..
```

### Committing
```bash
# Interactive commit (recommended)
uv run cz commit

# Or manual conventional commit
git commit -m "feat: add support for custom config paths"
git commit -m "fix: handle spaces in filenames correctly"
git commit -m "docs: update installation guide"
```

### Before Pushing
```bash
# Run all checks locally
uv run pre-commit run --all-files

# Test different configurations
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
```

## Common Tasks

### Update Version Manually
```bash
# See what would change
uv run cz bump --dry-run

# Bump version
uv run cz bump --changelog
```

### Run Specific Checks
```bash
# Just formatting
uv run pre-commit run clang-format --all-files

# Just static analysis
cppcheck -I include/ src/

# Validate commit message
echo "feat: my message" | uv run cz check --commit-msg-file -
```

### Test Package Building
```bash
cd build
make package
ls *.deb *.rpm *.tar.gz
```

### Run Unit Tests
```bash
# Quick test run
./scripts/run-tests.sh

# Run specific test suite
make test-path_operations

# Run with coverage report
mkdir build-coverage && cd build-coverage
cmake .. -DBUILD_TESTS=ON -DCMAKE_C_FLAGS="--coverage"
make && ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
# Open coverage-report/index.html
```

### Clean Everything
```bash
rm -rf build/ .venv/ __pycache__/
git clean -fdx  # WARNING: Removes all untracked files
```

## Commit Message Examples

### Features (Minor Version Bump)
```
feat: add --quiet option for silent operation
feat(trash): implement automatic cleanup after 30 days
feat!: change config file format to TOML (breaking)
```

### Fixes (Patch Version Bump)
```
fix: correctly handle symbolic links in trash mode
fix(protection): allow deletion of broken symlinks
fix: prevent segfault when HOME is not set
```

### Other (No Version Bump)
```
docs: add Chinese translation for README
style: format code according to clang-format
refactor: extract path validation into separate function
test: add unit tests for config parser
chore: update GitHub Actions to latest versions
ci: add Windows build to test matrix
build: upgrade CMake minimum version to 3.12
```

### Breaking Changes (Major Version Bump)
```
feat!: remove deprecated --preserve-links option

feat: new config format

BREAKING CHANGE: Config files must now use TOML format instead of INI
```

## Environment Variables

```bash
# Use custom trash directory
export BETTER_RM_TRASH=/mnt/backup/trash

# Skip version bump in CI
git commit -m "chore: update docs [skip ci]"

# Custom Python version with uv
UV_PYTHON_PREFERENCE=only-managed uv venv --python 3.9
```

## Debugging

### Build Issues
```bash
# Verbose CMake
cmake -B build --debug-output

# Verbose Make
cd build && make VERBOSE=1
```

### Pre-commit Issues
```bash
# See what hooks are installed
uv run pre-commit run --show-diff-on-failure

# Skip specific hook
SKIP=cppcheck uv run pre-commit run --all-files
```

### Version Bump Issues
```bash
# Check commit history parsing
uv run cz changelog --dry-run

# Validate version files
cat VERSION
grep VERSION include/version.h
```

## Tips

1. **Always use uv run**: Ensures correct Python environment
2. **Check CI status**: Before pushing, verify workflows will pass
3. **Small commits**: Easier to review and revert if needed
4. **Descriptive messages**: Help with automatic changelog
5. **Test locally**: Especially test packaging before releases

## Useful Aliases

Add to your shell config:
```bash
alias czc='uv run cz commit'
alias czb='uv run cz bump --changelog'
alias pre='uv run pre-commit run --all-files'
alias mkbuild='cmake -B build && cmake --build build'
```
