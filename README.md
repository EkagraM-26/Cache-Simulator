# Cache Simulator with Replacement Policy Comparison

A configurable **L1/L2 cache simulator** written in **C++17** that reads memory traces and compares **LRU**, **FIFO**, and **Random** replacement policies. Produces hit-rate statistics, MPKI, eviction counts, and visual comparison charts via Python/matplotlib.

> Built as a systems programming portfolio project targeting **NVIDIA**, **AMD**, **Intel**, and similar computer-architecture-focused roles.

---

## Features

| Feature | Details |
|---|---|
| **Cache levels** | Configurable L1 and L2 with inclusive/exclusive hierarchy |
| **Replacement policies** | LRU, FIFO, Random (easily extensible) |
| **Write policies** | Write-back + write-allocate (default) or write-through |
| **Trace formats** | Custom `R/W addr size`, Valgrind Lackey, PIN pinatrace |
| **Statistics** | Hit rate, miss rate, MPKI, evictions, dirty writebacks |
| **Visualization** | 5 matplotlib charts — bar graphs, 4-panel overview |
| **CLI** | Full argument parsing, compare mode, verbose mode |

---

## Project Structure

```
cache-simulator/
├── src/
│   ├── main.cpp                    # CLI entry point + argument parsing
│   ├── cache.hpp                   # CacheLine, CacheStats, Cache class
│   ├── cache_hierarchy.hpp         # L1→L2→memory builder
│   ├── trace_reader.hpp            # Multi-format trace parser + generator
│   └── policies/
│       ├── replacement_policy.hpp  # Abstract base class
│       ├── lru.hpp                 # Doubly-linked list LRU
│       ├── fifo.hpp                # Queue-based FIFO
│       └── random_policy.hpp       # Seeded MT19937 random
├── scripts/
│   └── visualize.py                # Matplotlib chart generator
├── traces/
│   ├── tiny.trace                  # 10-line test trace
│   └── sample.trace                # Auto-generated (500K accesses)
├── results/                        # CSV + PNG output (git-ignored)
├── Makefile
└── README.md
```

---

## Quick Start

### Prerequisites
- **g++** with C++17 support (`g++ --version` should be ≥ 7)
- **Python 3.8+** with matplotlib and numpy (for charts only)

```bash
# Install Python dependencies
pip install matplotlib numpy
```

### Build
```bash
make              # Compiles ./cache_sim
```

### Run — Single Policy
```bash
# Generate a sample 500K-access trace
./cache_sim --gen-trace traces/sample.trace

# Simulate LRU with default L1 (32KB, 8-way) + L2 (256KB, 16-way)
./cache_sim traces/sample.trace

# Specify a policy
./cache_sim --l1-policy FIFO traces/sample.trace
./cache_sim --l1-policy Random traces/sample.trace
```

### Run — Policy Comparison (main feature)
```bash
# Compare all 3 policies and write CSV
./cache_sim --compare traces/sample.trace

# Generate charts from the CSV
python3 scripts/visualize.py results/comparison.csv

# One-command full pipeline
make visualize
```

### Example Output
```
  ╔══════════════════════════════════════════╗
  ║     Cache Simulator v1.0                 ║
  ╚══════════════════════════════════════════╝

  Trace file : traces/sample.trace
  L1 Config  : 32 KB, 8-way, 64B blocks
  L2 Config  : 256 KB, 16-way, 64B blocks

  ── Running policy: LRU ──

  ╔═══ LRU — L1 ═══
  ║ Accesses :       500000
  ║ Hits     :       101529  (20.3058%)
  ║ Misses   :       398470  (79.6942%)
  ║ MPKI     :     796.9416
  ║ Evictions:       397958
  ╠── L2 ──
  ║ Hit Rate :      44.5698%
  ╚══════════════════════════════════════

  ┌────────────┬───────────────┬───────────────┬──────────────┐
  │  Policy    │  L1 Hit Rate  │  L1 MPKI      │  L2 Hit Rate │
  ├────────────┼───────────────┼───────────────┼──────────────┤
  │ LRU        │     20.3058%  │        796.94 │    44.5698%  │
  │ FIFO       │     20.3038%  │        796.96 │    44.5692%  │
  │ Random     │     25.9737%  │        740.26 │    41.1937%  │
  └────────────┴───────────────┴───────────────┴──────────────┘
```

---

## CLI Reference

```
Usage: ./cache_sim [options] <trace_file>

Options:
  --l1-size <bytes>     L1 cache size        (default: 32768 = 32 KB)
  --l1-assoc <N>        L1 associativity     (default: 8)
  --l1-block <bytes>    L1 block/line size   (default: 64)
  --l1-policy <P>       LRU | FIFO | Random  (default: LRU)
  --l2-size <bytes>     L2 cache size (0 = disabled, default: 262144)
  --l2-assoc <N>        L2 associativity     (default: 16)
  --l2-policy <P>       LRU | FIFO | Random  (default: LRU)
  --compare             Run all 3 policies and emit CSV
  --gen-trace <file>    Generate a synthetic trace file
  --output <file.csv>   CSV output path      (default: results/comparison.csv)
  --format <fmt>        auto | custom | valgrind | pin
  -v                    Verbose progress output
```

---

## Trace Formats

### Custom (default)
```
# R/W  hex_address  size_bytes
R 0x7fff5c400000 8
W 0x7fff5c400080 4
```

### Valgrind Lackey
```bash
valgrind --tool=lackey --trace-mem=yes ./your_program 2> traces/prog.trace
./cache_sim --format valgrind traces/prog.trace
```

### Intel PIN (pinatrace)
```bash
pin -t pinatrace.so -- ./your_program
./cache_sim --format pin pinatrace.out
```

---

## Architecture

### Cache Address Decoding

```
64-bit address:
 ┌─────────────────────┬──────────────┬─────────────┐
 │        TAG          │  SET INDEX   │   OFFSET    │
 │  (64-offset-index)  │  (log2 sets) │ (log2 block)│
 └─────────────────────┴──────────────┴─────────────┘
```

### Class Hierarchy

```
ReplacementPolicy  (abstract)
├── LRUPolicy      — per-set doubly-linked list, O(1) update
├── FIFOPolicy     — per-set FIFO queue
└── RandomPolicy   — seeded MT19937, deterministic replay

Cache              — set-associative, parameterised write policy
CacheHierarchy     — chains L1 → L2 → (implicit) main memory
TraceReader        — streaming parser, auto-detects format
```

---

## Extending the Simulator

### Add a New Replacement Policy
Create `src/policies/my_policy.hpp` implementing `ReplacementPolicy`:
```cpp
class OptimalPolicy : public ReplacementPolicy {
public:
    void on_access(int set, int way, uint64_t tag) override { /* ... */ }
    void on_insert(int set, int way, uint64_t tag) override { /* ... */ }
    int  get_evict_way(int set) override { /* ... */ }
    void reset() override { /* ... */ }
    std::string name() const override { return "Optimal"; }
};
```
Then register it in `cache_hierarchy.hpp`'s `make_policy()` factory.

---

## Stretch Goals (Resume Highlights)

- [ ] **Stride prefetcher** — detect stride patterns and prefetch N lines ahead
- [ ] **MESI protocol** — multi-core cache coherence (Modified/Exclusive/Shared/Invalid)
- [ ] **Victim cache** — fully-associative buffer between L1 and L2
- [ ] **Optimal (Belady's) policy** — offline algorithm, theoretical upper bound
- [ ] **Web dashboard** — real-time visualization served via a simple HTTP server

---

## Tools Used
- **C++17** — core simulator
- **Python 3 / matplotlib** — chart generation
- **Valgrind / Intel PIN** — real program memory traces
- **SPEC CPU 2017** — industry-standard benchmark traces (optional)

---

## License
MIT — free to use, modify, and include in your portfolio.
