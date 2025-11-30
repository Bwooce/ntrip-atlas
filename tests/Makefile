# NTRIP Atlas Test Suite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0
MATHLIB = -lm

# Directories
TEST_UNIT = unit
TEST_MEMORY = memory
TEST_INTEGRATION = integration

# Test executables
UNIT_TESTS = $(TEST_UNIT)/test_distance $(TEST_UNIT)/test_compact_failures
MEMORY_TESTS = $(TEST_MEMORY)/test_esp32_memory
INTEGRATION_TESTS =

ALL_TESTS = $(UNIT_TESTS) $(MEMORY_TESTS) $(INTEGRATION_TESTS)

# Default target
all: $(ALL_TESTS)

# Build individual test suites
$(TEST_UNIT)/test_distance: $(TEST_UNIT)/test_distance.c
	$(CC) $(CFLAGS) $< -o $@ $(MATHLIB)

$(TEST_UNIT)/test_compact_failures: $(TEST_UNIT)/test_compact_failures.c ../libntripatlas/src/ntrip_compact_failures.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_MEMORY)/test_esp32_memory: $(TEST_MEMORY)/test_esp32_memory.c
	$(CC) $(CFLAGS) $< -o $@ $(MATHLIB)

# Run all tests
test: $(ALL_TESTS)
	@echo "==============================="
	@echo "Running NTRIP Atlas Test Suite"
	@echo "==============================="
	@echo
	@echo "Unit Tests:"
	@echo "-----------"
	@$(TEST_UNIT)/test_distance || exit 1
	@echo
	@echo "Memory Tests:"
	@echo "-------------"
	@$(TEST_MEMORY)/test_esp32_memory || exit 1
	@echo
	@echo "🎉 All tests passed!"

# Run unit tests only
test-unit: $(UNIT_TESTS)
	@echo "Running unit tests..."
	@$(TEST_UNIT)/test_distance

# Run memory tests only
test-memory: $(MEMORY_TESTS)
	@echo "Running memory tests..."
	@$(TEST_MEMORY)/test_esp32_memory

# Validate YAML service files
validate-yaml:
	@echo "Validating YAML service files..."
	@python3 ../tools/validators/service_validator.py --directory ../data

# Clean build artifacts
clean:
	rm -f $(ALL_TESTS)
	rm -f $(TEST_UNIT)/*.o $(TEST_MEMORY)/*.o $(TEST_INTEGRATION)/*.o

# Continuous integration target
ci: all validate-yaml test

.PHONY: all test test-unit test-memory validate-yaml clean ci