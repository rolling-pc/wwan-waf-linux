# Overview

This software is designed for WWAN device tools and background services. The release package includes libraries and applications, compatible with Linux, Chrome OS, and Windows systems. It supports both x86_64 and aarch64 architectures.

## Features

- **Cross-Platform Support:** Compatible with multiple operating systems, including Linux, Chrome OS, and Windows.
- **Multi-Architecture Support:** Supports x86_64 and aarch64 CPU architectures.
- **Modular Design:** Libraries and applications are separated, with dynamic linking for easier maintenance and updates.

## Dependencies

To compile the software, the following tools are required:

- [CMake](https://cmake.org) (version 3.22 or later)
- [Ninja](https://ninja-build.org) build system

### Windows Dependencies

(Include specific dependencies if applicable, otherwise leave this section blank.)

### Linux Dependencies

#### On Ubuntu/Debian:

Install the required dependencies using:

```bash
sudo apt-get install -y g++ clang libc++-dev cmake ninja-build clang-format rpm clang-tidy
```
On Fedora:
```bash
    sudo dnf install -y gcc-c++ clang libcxx-devel cmake ninja-build clang-tidy
```

# Compiling

## linux

### 1. Configuration and Compilation:

-  First, configure the project and then complete the one-click compilation and packaging:
```bash
./build.sh config
```
### 2. Default Configuration:

-  To use the default configuration file, compile, and package the hp-rw135 service package, use the following command:
```bash
./build.sh hp-rw135-service-deb
```
### 3. test your code or lib:
```bash
./build.sh Test
```
# Additional Information
For further details or troubleshooting, please refer to the project documentation or contact the support team.
