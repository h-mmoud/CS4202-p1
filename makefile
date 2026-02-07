# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++23 -O3 -I./include

# Target executable
TARGET = cache-sim

# For deleting the target
TARGET_DEL = cache-sim.exe

# Source files
SRCS = cache-sim.cpp 

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default rule to build and run the executable
all: $(TARGET) run

# Rule to link object files into the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to run the executable
run: $(TARGET)
# 	./$(TARGET)

# Clean rule to remove generated files
clean:
	rm -rf $(TARGET_DEL) $(OBJS)
