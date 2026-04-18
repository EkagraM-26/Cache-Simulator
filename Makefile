# ─────────────────────────────────────────────────────────────────────────────
# Cache Simulator — Makefile
# Requires: g++ with C++17 support
# ─────────────────────────────────────────────────────────────────────────────

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Isrc
TARGET   := cache_sim
SRC      := src/main.cpp

.PHONY: all clean run run-compare gen-trace run-all help

all: $(TARGET)

$(TARGET): $(SRC) src/*.hpp src/policies/*.hpp
	@echo "  Compiling $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)
	@echo "  Build successful: ./$(TARGET)"

# ─── Quick run targets ───────────────────────────────────────────────────────

gen-trace: $(TARGET)
	@mkdir -p traces
	./$(TARGET) --gen-trace traces/sample.trace
	@echo "  Trace generated: traces/sample.trace"

run: gen-trace
	./$(TARGET) traces/sample.trace --l1-policy LRU -v

run-compare: gen-trace
	@mkdir -p results
	./$(TARGET) --compare -v traces/sample.trace

run-all: gen-trace
	@echo "\n=== LRU ==="
	./$(TARGET) --l1-policy LRU  traces/sample.trace
	@echo "\n=== FIFO ==="
	./$(TARGET) --l1-policy FIFO traces/sample.trace
	@echo "\n=== Random ==="
	./$(TARGET) --l1-policy Random traces/sample.trace

visualize: run-compare
	python3 scripts/visualize.py results/comparison.csv --output results

# ─── Custom config examples ──────────────────────────────────────────────────

run-l1only: gen-trace
	./$(TARGET) --compare --l2-size 0 traces/sample.trace

run-large-cache: gen-trace
	./$(TARGET) --compare --l1-size 131072 --l1-assoc 16 traces/sample.trace

clean:
	rm -f $(TARGET) traces/sample.trace results/comparison.csv
	rm -rf results/

help:
	@echo ""
	@echo "  Cache Simulator — Makefile Targets"
	@echo "  ────────────────────────────────────"
	@echo "  make              Build the simulator"
	@echo "  make run          Build, generate trace, run with LRU"
	@echo "  make run-compare  Compare LRU vs FIFO vs Random"
	@echo "  make visualize    Full pipeline + generate charts"
	@echo "  make run-all      Run each policy separately"
	@echo "  make run-l1only   Compare with L2 disabled"
	@echo "  make clean        Remove build artifacts"
	@echo ""
