# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -g -I/Users/hassano/CS4202/vcpkg_installed/arm64-osx/include

# Target executable
TARGET = helloworld

# For deleting the target
TARGET_DEL = helloworld.exe

# Source files
SRCS = helloworld.cpp 

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
