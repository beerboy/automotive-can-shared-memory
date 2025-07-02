#!/bin/bash

# Linear Probing Implementation Build and Test Script
# ==================================================

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Configuration
BUILD_DIR="build_linear_probing"
CMAKE_FILE="CMakeLists_linear_probing.txt"
BUILD_TYPE="Release"

# Parse command line arguments
CLEAN_BUILD=false
RUN_TESTS=true
RUN_DEMO=true
RUN_PERFORMANCE=true
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-tests)
            RUN_TESTS=false
            shift
            ;;
        --no-demo)
            RUN_DEMO=false
            shift
            ;;
        --no-performance)
            RUN_PERFORMANCE=false
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Linear Probing Build and Test Script"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean            Clean build directory before building"
            echo "  --debug            Build in Debug mode (default: Release)"
            echo "  --no-tests         Skip running basic tests"
            echo "  --no-demo          Skip running collision demo"
            echo "  --no-performance   Skip running performance tests"
            echo "  --verbose          Enable verbose output"
            echo "  --help, -h         Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                 # Build and run all tests"
            echo "  $0 --clean --debug # Clean build in debug mode"
            echo "  $0 --no-performance # Build and run tests but skip performance"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Function to check if running as root (needed for shared memory cleanup)
check_sudo() {
    if [ "$EUID" -eq 0 ]; then
        print_warning "Running as root. This is not recommended for normal operation."
    fi
}

# Function to clean up shared memory
cleanup_shared_memory() {
    print_status "Cleaning up shared memory..."
    sudo rm -f /dev/shm/can_data_shm || true
}

# Function to clean build directory
clean_build_dir() {
    if [ "$CLEAN_BUILD" = true ] || [ "$1" = "force" ]; then
        print_status "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi
}

# Function to create and configure build directory
setup_build_dir() {
    print_status "Setting up build directory..."
    
    if [ ! -f "$CMAKE_FILE" ]; then
        print_error "CMake file $CMAKE_FILE not found!"
        exit 1
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    print_status "Configuring CMake..."
    if [ "$VERBOSE" = true ]; then
        cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -f "../$CMAKE_FILE" ..
    else
        cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -f "../$CMAKE_FILE" .. > cmake_config.log 2>&1
        if [ $? -ne 0 ]; then
            print_error "CMake configuration failed. See build_linear_probing/cmake_config.log for details."
            exit 1
        fi
    fi
    
    print_success "CMake configuration completed"
}

# Function to build the project
build_project() {
    print_status "Building project in $BUILD_TYPE mode..."
    
    if [ "$VERBOSE" = true ]; then
        make -j$(nproc)
    else
        make -j$(nproc) > build.log 2>&1
        if [ $? -ne 0 ]; then
            print_error "Build failed. See build_linear_probing/build.log for details."
            exit 1
        fi
    fi
    
    print_success "Build completed successfully"
}

# Function to run tests
run_tests() {
    if [ "$RUN_TESTS" = true ]; then
        print_status "Running basic functionality tests..."
        cleanup_shared_memory
        
        echo ""
        echo "========================================"
        echo "         BASIC FUNCTIONALITY TESTS"
        echo "========================================"
        ./test_linear_probing
        
        print_success "Basic tests completed"
    fi
}

# Function to run collision demonstration
run_demo() {
    if [ "$RUN_DEMO" = true ]; then
        print_status "Running collision demonstration..."
        cleanup_shared_memory
        
        echo ""
        echo "========================================"
        echo "         COLLISION DEMONSTRATION"
        echo "========================================"
        ./demo_collision_comparison
        
        print_success "Collision demo completed"
    fi
}

# Function to run performance tests
run_performance() {
    if [ "$RUN_PERFORMANCE" = true ]; then
        print_status "Running performance tests..."
        cleanup_shared_memory
        
        echo ""
        echo "========================================"
        echo "           PERFORMANCE TESTS"
        echo "========================================"
        ./performance_test
        
        print_success "Performance tests completed"
    fi
}

# Function to summarize results
print_summary() {
    echo ""
    echo "========================================"
    echo "              SUMMARY"
    echo "========================================"
    print_success "Linear Probing Implementation Build Complete!"
    echo ""
    echo "Built executables:"
    echo "  - test_linear_probing:      Basic functionality tests"
    echo "  - demo_collision_comparison: Hash collision demonstration"
    echo "  - performance_test:         Performance benchmarks"
    echo ""
    echo "Key benefits demonstrated:"
    echo "  ✓ No data loss on hash collisions"
    echo "  ✓ Consistent O(1) performance for reasonable load factors"
    echo "  ✓ Safe concurrent access with seqlock"
    echo "  ✓ Detailed statistics for monitoring"
    echo ""
    
    if [ -f "build.log" ] && [ "$VERBOSE" = false ]; then
        echo "Build logs available in: $BUILD_DIR/build.log"
    fi
    
    if [ -f "cmake_config.log" ] && [ "$VERBOSE" = false ]; then
        echo "CMake logs available in: $BUILD_DIR/cmake_config.log"
    fi
    
    echo ""
    echo "To run individual tests:"
    echo "  cd $BUILD_DIR"
    echo "  ./test_linear_probing"
    echo "  ./demo_collision_comparison"
    echo "  ./performance_test"
    echo "========================================"
}

# Main execution
main() {
    echo "Linear Probing Implementation - Build and Test"
    echo "=============================================="
    echo "Build Type: $BUILD_TYPE"
    echo "Clean Build: $CLEAN_BUILD"
    echo "Run Tests: $RUN_TESTS"
    echo "Run Demo: $RUN_DEMO"
    echo "Run Performance: $RUN_PERFORMANCE"
    echo "Verbose: $VERBOSE"
    echo ""
    
    # Check prerequisites
    check_sudo
    
    # Check if required commands are available
    command -v cmake >/dev/null 2>&1 || { print_error "cmake is required but not installed. Aborting."; exit 1; }
    command -v make >/dev/null 2>&1 || { print_error "make is required but not installed. Aborting."; exit 1; }
    command -v gcc >/dev/null 2>&1 || { print_error "gcc is required but not installed. Aborting."; exit 1; }
    
    # Start build process
    clean_build_dir
    setup_build_dir
    build_project
    
    # Run tests
    run_tests
    run_demo
    run_performance
    
    # Return to original directory
    cd ..
    
    # Print summary
    print_summary
}

# Trap to cleanup on exit
trap cleanup_shared_memory EXIT

# Run main function
main "$@"