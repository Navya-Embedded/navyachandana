# Compiler and Flags
CC = gcc
# Add include paths for all source directories
CFLAGS = -Wall -Wextra -Os -std=c11 -I./src -I./src/drivers -I./test
# Add a special flag for test builds to enable test-only code
TEST_CFLAGS = $(CFLAGS) -DUNIT_TESTING

# --- Directories ---
OBJ_DIR = obj
# VPATH tells 'make' where to find source files
VPATH = src src/drivers test

# --- Application Build ---
# List all source files needed for the main application
# This should NOT include the test runner's entry point (test_app.c)
APP_SRCS = main.c tsmon_app.c tsmon_hal.c temp_sensor.c
# Generate object file paths in the obj/ directory
APP_OBJS = $(addprefix $(OBJ_DIR)/, $(APP_SRCS:.c=.o))
APP_TARGET = tsmon_firmware

# --- Test Build ---
# List all source files needed for the test build
# This should NOT include the main application's entry point (main.c)
TEST_SRCS = test_app.c tsmon_app.c tsmon_hal.c temp_sensor.c
# Generate object file paths in the obj/ directory
TEST_OBJS = $(addprefix $(OBJ_DIR)/, $(TEST_SRCS:.c=.o))
TEST_TARGET = test_runner


# --- Build Rules ---

# Default target: build the main application
all: $(APP_TARGET)

# Rule to build and run the unit tests
test: $(TEST_TARGET)
	@echo "--- Running Tests ---"
	./$(TEST_TARGET)

# Rule to link the main application
$(APP_TARGET): $(APP_OBJS)
	@echo "Linking application: $@"
	$(CC) -o $@ $^

# Rule to link the test runner
$(TEST_TARGET): $(TEST_OBJS)
	@echo "Linking test runner: $@"
	$(CC) -o $@ $^

# Rule to compile a source file that needs the UNIT_TESTING flag
$(OBJ_DIR)/tsmon_hal.o: tsmon_hal.c | $(OBJ_DIR)
	@echo "Compiling with test flags: $<"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Rule to compile any other source file
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Rule to clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(APP_TARGET) $(TEST_TARGET)

# Declare targets that are not files
.PHONY: all test clean