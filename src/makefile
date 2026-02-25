# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -O3 -g -I./include

# Directories
BIN_DIR = bin

# Target executable
TARGET = cache-sim

# Source files
SRCS = main.cpp cache.cpp config.cpp trace.cpp

# Object files (in bin directory)
OBJS = $(SRCS:%.cpp=$(BIN_DIR)/%.o)

# Header files
HDRS = include/cache.hpp include/config.hpp include/trace.hpp

# Default rule to build and run the executable
all: $(TARGET) run

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Rule to link object files into the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile .cpp files into .o files in bin directory
$(BIN_DIR)/%.o: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to run the executable
run: $(TARGET)
# 	./$(TARGET)

# Dependencies
$(BIN_DIR)/main.o: main.cpp include/cache.hpp include/config.hpp include/trace.hpp
$(BIN_DIR)/cache.o: cache.cpp include/cache.hpp
$(BIN_DIR)/config.o: config.cpp include/config.hpp include/cache.hpp
$(BIN_DIR)/trace.o: trace.cpp include/trace.hpp

# Clean rule to remove generated files
clean:
	rm -rf $(BIN_DIR) $(TARGET)

.PHONY: all run clean