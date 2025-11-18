# Compiler and flags
CXX      := g++
CXXFLAGS := -std=c++17 -g -Og -Wall -Iinclude

# Directories
SRC_DIR  := src
INC_DIR  := include
BUILD_DIR := build
BIN_DIR   := bin

LIBPIMEVAL := ../PIM-AutoDSE/libpimeval/lib/libpimeval.a

# Target executable
TARGET := $(BIN_DIR)/app

# Find all .cpp files in src/
SRCS := $(wildcard $(SRC_DIR)/*.cpp)

# Map src/foo.cpp → build/foo.o
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Map objects → dependency files
DEPS := $(OBJS:.o=.d)

.PHONY: all clean dirs

all: dirs $(TARGET)

# Link the final executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LIBPIMEVAL) -o $@

# Compile each .cpp → .o and produce .d dependency file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp Makefile
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Create directories if they don't exist
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Include automatically generated dependencies
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

