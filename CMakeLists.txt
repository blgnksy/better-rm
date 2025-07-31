cmake_minimum_required(VERSION 3.10)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})
set(VERSION_PATCH ${CMAKE_MATCH_3})

# Define project with version
project(better-rm VERSION ${VERSION_STRING} LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add executable
add_executable(better-rm src/main.c)

# Set compile options
target_compile_options(better-rm PRIVATE -Wall -Wextra -Wpedantic)

# Define GNU_SOURCE for Linux-specific features
target_compile_definitions(better-rm PRIVATE _GNU_SOURCE)

# Pass version information to the compiler
target_compile_definitions(better-rm PRIVATE
        VERSION="${VERSION_STRING}"
        VERSION_MAJOR=${VERSION_MAJOR}
        VERSION_MINOR=${VERSION_MINOR}
        VERSION_PATCH=${VERSION_PATCH}
)

# Include directories
target_include_directories(better-rm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Installation rules
include(GNUInstallDirs)

# Install the binary
install(TARGETS better-rm
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration file
install(FILES config/better-rm.conf.example
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
        RENAME better-rm.conf.example
)


include(CPack)

# Print version information
message(STATUS "Building ${PROJECT_NAME} version ${VERSION_STRING}")