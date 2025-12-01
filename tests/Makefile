# NTRIP Atlas Test Suite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0
MATHLIB = -lm

# Directories
TEST_UNIT = unit
TEST_MEMORY = memory
TEST_INTEGRATION = integration

# Test executables
UNIT_TESTS = $(TEST_UNIT)/test_distance $(TEST_UNIT)/test_compact_failures $(TEST_UNIT)/test_database_versioning $(TEST_UNIT)/test_credential_management $(TEST_UNIT)/test_compact_services $(TEST_UNIT)/test_geographic_blacklist $(TEST_UNIT)/test_geographic_filtering $(TEST_UNIT)/test_spatial_indexing $(TEST_UNIT)/test_yaml_generated_services
MEMORY_TESTS = $(TEST_MEMORY)/test_esp32_memory
INTEGRATION_TESTS =

ALL_TESTS = $(UNIT_TESTS) $(MEMORY_TESTS) $(INTEGRATION_TESTS)

# Default target
all: generate-services $(ALL_TESTS)

# Generate C code from YAML service definitions
generate-services:
	@echo "Generating services from YAML..."
	@python3 ../tools/generators/yaml_to_c.py ../data/ ../libntripatlas/src/generated/

# Generated services dependency
../libntripatlas/src/generated/ntrip_generated_services.c: ../data/**/*.yaml
	@$(MAKE) generate-services

# Build individual test suites
$(TEST_UNIT)/test_distance: $(TEST_UNIT)/test_distance.c
	$(CC) $(CFLAGS) $< -o $@ $(MATHLIB)

$(TEST_UNIT)/test_compact_failures: $(TEST_UNIT)/test_compact_failures.c ../libntripatlas/src/ntrip_compact_failures.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_database_versioning: $(TEST_UNIT)/test_database_versioning.c ../libntripatlas/src/ntrip_versioning.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_credential_management: $(TEST_UNIT)/test_credential_management.c ../libntripatlas/src/ntrip_credential_management.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_compact_services: $(TEST_UNIT)/test_compact_services.c ../libntripatlas/src/ntrip_compact_services.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_geographic_blacklist: $(TEST_UNIT)/test_geographic_blacklist.c ../libntripatlas/src/ntrip_geographic_blacklist.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_geographic_filtering: $(TEST_UNIT)/test_geographic_filtering.c ../libntripatlas/src/ntrip_geographic_filtering.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_spatial_indexing: $(TEST_UNIT)/test_spatial_indexing.c ../libntripatlas/src/ntrip_spatial_indexing.c
	$(CC) $(CFLAGS) -I../libntripatlas/include $^ -o $@ $(MATHLIB)

$(TEST_UNIT)/test_yaml_generated_services: $(TEST_UNIT)/test_yaml_generated_services.c ../libntripatlas/src/generated/ntrip_generated_services.c ../libntripatlas/src/ntrip_geographic_filtering.c ../libntripatlas/src/ntrip_spatial_indexing.c ../libntripatlas/src/ntrip_spatial_geographic.c
	$(CC) $(CFLAGS) -I../libntripatlas/include -I../libntripatlas/src/generated $^ -o $@ $(MATHLIB)

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
	@$(TEST_UNIT)/test_compact_failures || exit 1
	@$(TEST_UNIT)/test_database_versioning || exit 1
	@$(TEST_UNIT)/test_credential_management || exit 1
	@$(TEST_UNIT)/test_compact_services || exit 1
	@$(TEST_UNIT)/test_geographic_blacklist || exit 1
	@$(TEST_UNIT)/test_geographic_filtering || exit 1
	@$(TEST_UNIT)/test_spatial_indexing || exit 1
	@$(TEST_UNIT)/test_yaml_generated_services || exit 1
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