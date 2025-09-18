# Mandelbrot Set Visualizer Makefile
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -ffast-math -march=native -mtune=native -funroll-loops -fomit-frame-pointer
INCLUDES = -IC:/raylib/raylib/src
LIBDIRS = -LC:/raylib/raylib/src
LIBS = -lraylib -lgdi32 -lwinmm

# Source and target
SOURCE = MandelBrot.cpp
TARGET = mandelbrot_optimized.exe
TARGET_DEBUG = mandelbrot_debug.exe

# Default target
all: $(TARGET)

# Optimized release build
$(TARGET): $(SOURCE)
	@echo "Building optimized Mandelbrot visualizer..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBDIRS) $(SOURCE) $(LIBS) -o $(TARGET)
	@echo "Build complete! Run with: ./$(TARGET)"

# Debug build with symbols
debug: $(SOURCE)
	@echo "Building debug version..."
	$(CXX) -std=c++17 -g -O0 -DDEBUG $(INCLUDES) $(LIBDIRS) $(SOURCE) $(LIBS) -o $(TARGET_DEBUG)
	@echo "Debug build complete! Run with: ./$(TARGET_DEBUG)"

# Quick build (less optimized but faster compilation)
quick: $(SOURCE)
	@echo "Building quick version..."
	$(CXX) -std=c++17 -O2 $(INCLUDES) $(LIBDIRS) $(SOURCE) $(LIBS) -o $(TARGET)
	@echo "Quick build complete!"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@if exist $(TARGET) del /Q $(TARGET)
	@if exist $(TARGET_DEBUG) del /Q $(TARGET_DEBUG)
	@if exist *.o del /Q *.o
	@echo "Clean complete!"

# Install raylib (helper target)
install-raylib:
	@echo "Please download and install raylib from: https://github.com/raysan5/raylib/releases"
	@echo "Extract to C:/raylib/ directory"

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build optimized release version (default)"
	@echo "  debug    - Build debug version with symbols"
	@echo "  quick    - Build with moderate optimizations (faster compile)"
	@echo "  run      - Build and run the program"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"

.PHONY: all debug quick run clean install-raylib help
