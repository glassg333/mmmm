CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pthread

TEST_TARGETS = tests/test_full_suite tests/test_fm_dyn tests/test_bbox tests/test_arp tests/test_mod_matrix tests/test_conversions

all: $(TEST_TARGETS)

tests/%: tests/%.cpp
	$(CXX) $(CXXFLAGS) -I. -Idsp -Imodels -Iui $< -o $@

test: all
	@echo "=== Running Monomachine Architecture Verification Tests ==="
	@tests/test_full_suite
	@tests/test_fm_dyn
	@tests/test_bbox
	@tests/test_arp
	@tests/test_mod_matrix
	@tests/test_conversions
	@echo "=== All Tests Passed! ==="

clean:
	rm -f $(TEST_TARGETS)
