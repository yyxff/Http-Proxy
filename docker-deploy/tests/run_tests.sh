#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Get the directory of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

check_install_dependencies() {
    echo "Checking dependencies..."
    
    # cmake
    if ! command -v cmake &> /dev/null; then
        echo "Installing cmake..."
        sudo apt-get update
        sudo apt-get install -y cmake
    fi
    
    # g++
    if ! command -v g++ &> /dev/null; then
        echo "Installing g++..."
        sudo apt-get install -y g++
    fi
    
    # Google Test
    if ! dpkg -l | grep libgtest-dev &> /dev/null; then
        echo "Installing Google Test..."
        sudo apt-get install -y libgtest-dev
        cd /usr/src/gtest
        sudo cmake CMakeLists.txt
        sudo make
        sudo cp lib/*.a /usr/lib
        cd -
    fi
}

echo "Starting proxy tests..."

check_install_dependencies

# Create necessary directories
mkdir -p "$PROJECT_ROOT/var/log/erss"

# Create build directory in tests folder
cd "$SCRIPT_DIR"
mkdir -p build
cd build

echo "Building project..."
cmake ..
make

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo "Creating test log directory..."
mkdir -p "$SCRIPT_DIR/build/test_logs"
chmod 777 "$SCRIPT_DIR/build/test_logs"

echo "Running tests..."
./proxy_test

if [ $? -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

echo "Creating log directory..."
mkdir -p "$SCRIPT_DIR/../var/log/erss"
chmod 777 "$SCRIPT_DIR/../var/log/erss" 
