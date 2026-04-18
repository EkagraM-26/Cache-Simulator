/**
 * Cache Simulator — Main Entry Point
 *
 * Usage:
 *   ./cache_sim [options] <trace_file>
 *
 * Options:
 *   --l1-size <bytes>       L1 cache size (default: 32768)
 *   --l1-assoc <N>          L1 associativity (default: 8)
 *   --l1-block <bytes>      L1 block size (default: 64)
 *   --l1-policy <name>      L1 replacement policy: LRU|FIFO|Random (default: LRU)
 *   --l2-size <bytes>       L2 cache size (0 = disable, default: 262144)
 *   --l2-assoc <N>          L2 associativity (default: 16)
 *   --l2-policy <name>      L2 replacement policy (default: LRU)
 *   --compare               Run all 3 policies and emit CSV results
 *   --gen-trace <file>      Generate a sample trace file and exit
 *   --output <csv>          Write results to CSV file
 *   --format <fmt>          Trace format: auto|custom|valgrind|pin (default: auto)
 *   -v                      Verbose: print per-1M-access progress
 */

#include "cache_hierarchy.hpp"
#include "trace_reader.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdlib>

// ─── Helpers ────────────────────────────────────────────────────────────────

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options] <trace_file>\n\n"
        << "Options:\n"
        << "  --l1-size <bytes>     L1 size (default 32768)\n"
        << "  --l1-assoc <N>        L1 ways  (default 8)\n"
        << "  --l1-block <bytes>    Block size (default 64)\n"
        << "  --l1-policy <P>       LRU|FIFO|Random (default LRU)\n"
        << "  --l2-size <bytes>     L2 size (0 = off, default 262144)\n"
        << "  --l2-assoc <N>        L2 ways  (default 16)\n"
        << "  --l2-policy <P>       LRU|FIFO|Random (default LRU)\n"
        << "  --compare             Compare all 3 policies, output CSV\n"
        << "  --gen-trace <file>    Generate sample trace and exit\n"
        << "  --output <file.csv>   Write CSV results\n"
        << "  --format <fmt>        auto|custom|valgrind|pin\n"
        << "  -v                    Verbose progress\n\n"
        << "Examples:\n"
        << "  " << prog << " traces/sample.trace\n"
        << "  " << prog << " --compare --l1-size 65536 traces/sample.trace\n"
        << "  " << prog << " --gen-trace traces/my.trace\n";
}

static std::string fmt_bytes(uint64_t b) {
    if (b >= 1024*1024) return std::to_string(b/(1024*1024)) + " MB";
    if (b >= 1024)      return std::to_string(b/1024)        + " KB";
    return std::to_string(b) + "  B";
}

static void print_stats(const std::string& label,
                        const CacheStats& s1,
                        const CacheStats* s2 = nullptr) {
    auto bar = [](double pct, int width = 30) {
        int filled = static_cast<int>(pct * width);
        std::string result;
        for (int i = 0; i < filled; i++)      result += "#";
        for (int i = filled; i < width; i++)  result += ".";
        return result;
    };

    std::cout << "\n  ╔═══ " << label << " ═══\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  ║ Accesses : " << std::setw(12) << s1.accesses << "\n";
    std::cout << "  ║ Hits     : " << std::setw(12) << s1.hits
              << "  (" << std::setw(7) << s1.hit_rate()*100 << "%)\n";
    std::cout << "  ║ Misses   : " << std::setw(12) << s1.misses
              << "  (" << std::setw(7) << s1.miss_rate()*100 << "%)\n";
    std::cout << "  ║ MPKI     : " << std::setw(12) << s1.mpki() << "\n";
    std::cout << "  ║ Evictions: " << std::setw(12) << s1.evictions << "\n";
    std::cout << "  ║ Writebacks: " << std::setw(11) << s1.writebacks << "\n";
    std::cout << "  ║\n";
    std::cout << "  ║ Hit Rate  [" << bar(s1.hit_rate()) << "] "
              << std::setprecision(1) << s1.hit_rate()*100 << "%\n";
    std::cout << "  ║ Miss Rate [" << bar(s1.miss_rate()) << "] "
              << s1.miss_rate()*100 << "%\n";

    if (s2 && s2->accesses > 0) {
        std::cout << "  ╠── L2 ──\n";
        std::cout << std::setprecision(4);
        std::cout << "  ║ Accesses : " << std::setw(12) << s2->accesses << "\n";
        std::cout << "  ║ Hit Rate : " << std::setw(12) << s2->hit_rate()*100 << "%\n";
        std::cout << "  ║ MPKI     : " << std::setw(12) << s2->mpki() << "\n";
    }
    std::cout << "  ╚══════════════════════════════════════\n";
}

static void write_csv(const std::string& path,
                      const std::vector<std::string>& policies,
                      const std::vector<CacheStats>& l1_stats,
                      const std::vector<CacheStats>& l2_stats) {
    std::ofstream f(path);
    f << "Policy,L1_Accesses,L1_Hits,L1_Misses,L1_HitRate,L1_MissRate,"
         "L1_MPKI,L1_Evictions,L1_Writebacks,"
         "L2_Accesses,L2_Hits,L2_Misses,L2_HitRate,L2_MPKI\n";

    for (size_t i = 0; i < policies.size(); i++) {
        auto& s1 = l1_stats[i];
        auto& s2 = l2_stats[i];
        f << policies[i]      << ","
          << s1.accesses      << "," << s1.hits     << "," << s1.misses     << ","
          << s1.hit_rate()    << "," << s1.miss_rate() << "," << s1.mpki()  << ","
          << s1.evictions     << "," << s1.writebacks  << ","
          << s2.accesses      << "," << s2.hits     << "," << s2.misses     << ","
          << s2.hit_rate()    << "," << s2.mpki()   << "\n";
    }
    std::cout << "\n  CSV results written to: " << path << "\n";
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    // ── Defaults ──
    HierarchyConfig hcfg;
    std::string     trace_file;
    std::string     output_csv  = "results/comparison.csv";
    std::string     trace_fmt   = "auto";
    std::string     gen_trace;
    bool            compare     = false;
    bool            verbose     = false;

    // ── Parse args ──
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (++i >= argc) { std::cerr << "Missing value for " << a << "\n"; exit(1); }
            return argv[i];
        };

        if      (a == "--l1-size")   hcfg.l1_size     = std::stoull(next());
        else if (a == "--l1-assoc")  hcfg.l1_assoc    = std::stoul(next());
        else if (a == "--l1-block")  hcfg.l1_block     = std::stoul(next());
        else if (a == "--l1-policy") hcfg.l1_policy    = next();
        else if (a == "--l2-size")   hcfg.l2_size      = std::stoull(next());
        else if (a == "--l2-assoc")  hcfg.l2_assoc     = std::stoul(next());
        else if (a == "--l2-policy") hcfg.l2_policy    = next();
        else if (a == "--output")    output_csv         = next();
        else if (a == "--format")    trace_fmt          = next();
        else if (a == "--gen-trace") gen_trace          = next();
        else if (a == "--compare")   compare            = true;
        else if (a == "-v")          verbose            = true;
        else if (a == "--help" || a == "-h") { print_usage(argv[0]); return 0; }
        else if (a[0] != '-')        trace_file         = a;
        else { std::cerr << "Unknown option: " << a << "\n"; return 1; }
    }

    // ── Generate trace mode ──
    if (!gen_trace.empty()) {
        std::cout << "Generating sample trace: " << gen_trace << "\n";
        TraceReader::generate_sample(gen_trace, 500000, 1 << 20);
        std::cout << "Done. 500,000 accesses written.\n";
        return 0;
    }

    if (trace_file.empty()) {
        std::cerr << "Error: no trace file specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    // ── Select trace format ──
    TraceFormat fmt = TraceFormat::AUTO;
    if (trace_fmt == "custom")   fmt = TraceFormat::CUSTOM;
    if (trace_fmt == "valgrind") fmt = TraceFormat::VALGRIND;
    if (trace_fmt == "pin")      fmt = TraceFormat::PIN;

    // ── Banner ──
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════╗\n";
    std::cout << "  ║     Cache Simulator v1.0                 ║\n";
    std::cout << "  ╚══════════════════════════════════════════╝\n\n";
    std::cout << "  Trace file : " << trace_file << "\n";
    std::cout << "  L1 Config  : " << fmt_bytes(hcfg.l1_size)
              << ", " << hcfg.l1_assoc << "-way, "
              << hcfg.l1_block << "B blocks\n";
    if (hcfg.l2_size > 0)
        std::cout << "  L2 Config  : " << fmt_bytes(hcfg.l2_size)
                  << ", " << hcfg.l2_assoc << "-way, "
                  << hcfg.l1_block << "B blocks\n";

    // ── Compare mode: run LRU, FIFO, Random ──
    if (compare) {
        std::vector<std::string> policies = {"LRU", "FIFO", "Random"};
        std::vector<CacheStats>  l1_stats, l2_stats;

        for (auto& p : policies) {
            hcfg.l1_policy = p;
            hcfg.l2_policy = p;
            auto hierarchy = build_hierarchy(hcfg);

            std::cout << "\n  ── Running policy: " << p << " ──\n";
            auto t0 = std::chrono::steady_clock::now();

            TraceReader reader(trace_file, fmt);
            uint64_t n = 0;
            reader.read([&](const MemAccess& acc) {
                hierarchy.access(acc.addr, acc.is_write);
                if (verbose && ++n % 1'000'000 == 0)
                    std::cout << "    " << n/1000000 << "M accesses...\n";
            });

            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double,std::milli>(t1-t0).count();
            std::cout << "  Completed " << n << " accesses in "
                      << std::fixed << std::setprecision(1) << ms << " ms\n";

            l1_stats.push_back(hierarchy.l1->stats());
            l2_stats.push_back(hierarchy.l2 ? hierarchy.l2->stats() : CacheStats{});

            print_stats(p + " — L1", l1_stats.back(),
                        hierarchy.l2 ? &l2_stats.back() : nullptr);
        }

        // Summary comparison table
        std::cout << "\n  ┌────────────┬───────────────┬───────────────┬──────────────┐\n";
        std::cout <<   "  │  Policy    │  L1 Hit Rate  │  L1 MPKI      │  L2 Hit Rate │\n";
        std::cout <<   "  ├────────────┼───────────────┼───────────────┼──────────────┤\n";
        for (size_t i = 0; i < policies.size(); i++) {
            auto& s1 = l1_stats[i]; auto& s2 = l2_stats[i];
            std::cout << "  │ " << std::left  << std::setw(10) << policies[i]
                      << " │ " << std::right << std::setw(11) << std::fixed
                      << std::setprecision(4) << s1.hit_rate()*100 << "% "
                      << " │ " << std::setw(13) << std::setprecision(2) << s1.mpki()
                      << " │ " << std::setw(10) << std::setprecision(4)
                      << (s2.accesses ? s2.hit_rate()*100 : 0.0) << "% │\n";
        }
        std::cout <<   "  └────────────┴───────────────┴───────────────┴──────────────┘\n";

        // Write CSV
        [[maybe_unused]] int _r = std::system("mkdir -p results");
        write_csv(output_csv, policies, l1_stats, l2_stats);
        std::cout << "\n  Run: python3 scripts/visualize.py " << output_csv
                  << " -- to generate comparison charts\n\n";

    } else {
        // ── Single policy mode ──
        auto hierarchy = build_hierarchy(hcfg);

        std::cout << "  Policy     : " << hcfg.l1_policy << "\n\n";
        auto t0 = std::chrono::steady_clock::now();

        TraceReader reader(trace_file, fmt);
        uint64_t n = 0;
        reader.read([&](const MemAccess& acc) {
            hierarchy.access(acc.addr, acc.is_write);
            if (verbose && ++n % 1'000'000 == 0)
                std::cout << "  " << n/1000000 << "M accesses...\n";
        });

        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double,std::milli>(t1-t0).count();
        std::cout << "  Completed " << n << " accesses in "
                  << std::fixed << std::setprecision(1) << ms << " ms\n";

        print_stats(hcfg.l1_policy + " — L1", hierarchy.l1->stats(),
                    hierarchy.l2 ? &hierarchy.l2->stats() : nullptr);
    }

    return 0;
}
