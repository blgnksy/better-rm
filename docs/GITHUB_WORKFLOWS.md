# GitHub Workflows Documentation

This document describes the GitHub Actions workflows used in the better-rm project.

## Workflows Overview

### 1. Pre-commit Checks (`.github/workflows/pre-commit.yml`)

**Triggers**: On every pull request

**Purpose**: Ensures code quality and consistency

**What it does**:
- Sets up Python environment using uv (reads Python version from `.python-version`)
- Installs system dependencies (cppcheck, clang-format)
- Runs all pre-commit hooks except commitizen:
    - Code formatting (clang-format)
    - Static analysis (cppcheck)
    - File checks (trailing whitespace, YAML validation)
    - Python linting (ruff)

### 2. Build and Test (`.github/workflows/build.yml`)

**Triggers**:
- On pull requests
- Manual trigger via workflow_dispatch

**Purpose**: Ensures the code builds and works correctly

**Matrix Testing**:
- Compilers: gcc, clang
- Build types: Debug, Release

**What it does**:
- Builds the project with different configurations
- Runs static analysis with cppcheck
- Tests installation (binary name: `better-rm`)
- Runs basic functionality tests:
    - Version and help commands
    - Dry-run mode testing
    - Trash mode functionality
    - System directory protection

### 3. Version Bump (`.github/workflows/bump-version.yml`)

**Triggers**: On push to master branch

**Purpose**: Automatic semantic versioning based on conventional commits

**What it does**:
- Checks if version bump is needed by analyzing commits since last tag
- Looks for conventional commit prefixes (feat, fix, perf, BREAKING CHANGE)
- Runs commitizen to bump version if needed
- Updates VERSION and CHANGELOG.md files
- Creates and pushes git tag
- Creates GitHub Release with changelog

**Skip Conditions**:
- Commits containing `[skip ci]`
- Commits containing `bump:`

**Required Secrets**:
- `BR_PAT`: Personal Access Token for pushing changes and creating releases

### 4. Documentation Publishing (`.github/workflows/publish_docs.yml`)

**Triggers**:
- On push to master branch
- Manual trigger via workflow_dispatch

**Purpose**: Build and publish Doxygen documentation to GitHub Pages

**What it does**:
- Installs documentation dependencies (cmake, doxygen, graphviz)
- Configures CMake with BUILD_WITH_DOCS=ON
- Builds Doxygen documentation
- Adds .nojekyll file for proper GitHub Pages rendering
- Deploys HTML documentation to GitHub Pages

**Environment**:
- Name: github-pages
- URL: Available at deployment output URL

### 5. Release (`.github/workflows/release.yml`)

**Triggers**: On pushing version tags (v*)

**Purpose**: Build and publish release artifacts

**What it does**:
- Builds optimized release binary (`better-rm`)
- Creates distribution packages:
    - .deb: `better-rm_${VERSION}_amd64.deb`
    - .rpm: `better-rm-${VERSION}-1.x86_64.rpm`
    - .tar.gz: `better-rm-${VERSION}.tar.gz`
- Creates platform-specific binary tarball with:
    - Binary executable
    - README.md
    - Config files
    - Systemd files
- Uploads all artifacts to GitHub Release

**Required Secrets**:
- `BR_PAT`: Personal Access Token for uploading release assets

## Development Flow

### Regular Development

1. Developer creates feature branch
2. Makes changes and commits using conventional format
3. Opens PR → Pre-commit and Build workflows run
4. After review, PR is merged to master
5. Version Bump workflow automatically:
    - Analyzes commits
    - Bumps version if needed
    - Creates tag and release
6. Documentation is automatically published to GitHub Pages

### Manual Release

If you need to create a release manually:

```bash
# Local version bump
uv run cz bump --changelog

# Push changes and tag
git push origin master --follow-tags
```

## Workflow Dependencies

### Required Secrets
- `GITHUB_TOKEN`: Automatically provided by GitHub Actions
- `BR_PAT`: Personal Access Token with repo permissions for:
    - Pushing commits (version bump)
    - Creating releases
    - Uploading release assets

### Required Files
- `.python-version`: Specifies Python version for uv
- `pyproject.toml`: Python dependencies (pre-commit, commitizen, ruff)
- `.cz.toml`: Commitizen configuration
- `VERSION`: Current version number
- `include/version.h`: C header with version (updated by commitizen)
- `docs/Doxyfile.in`: Doxygen configuration template
