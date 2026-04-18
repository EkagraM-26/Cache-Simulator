#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <iomanip>

// ─── A single memory access from the trace ─────────────────────────────────
struct MemAccess {
    uint64_t addr;
    bool     is_write;   // true = store, false = load
    uint32_t size;       // access size in bytes (often 1, 4, or 8)
};

// ─── Supported trace formats ────────────────────────────────────────────────
enum class TraceFormat {
    AUTO,       // Detect automatically
    CUSTOM,     // Our simple "R/W addr size" format
    VALGRIND,   // Lackey tool output:  I/L/S/M addr,size
    PIN,        // Pinatrace output:    R/W addr
};

// ─── Trace reader ────────────────────────────────────────────────────────────
class TraceReader {
public:
    TraceReader(const std::string& filepath, TraceFormat fmt = TraceFormat::AUTO)
        : filepath_(filepath), fmt_(fmt) {}

    // Iterate over all accesses, calling cb for each one.
    // Returns total number of accesses read.
    uint64_t read(std::function<void(const MemAccess&)> cb) {
        std::ifstream f(filepath_);
        if (!f.is_open())
            throw std::runtime_error("Cannot open trace file: " + filepath_);

        if (fmt_ == TraceFormat::AUTO)
            fmt_ = detect_format(f);

        uint64_t count = 0;
        std::string line;

        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue; // skip comments

            MemAccess acc{};
            bool ok = false;

            switch (fmt_) {
                case TraceFormat::CUSTOM:   ok = parse_custom(line, acc);   break;
                case TraceFormat::VALGRIND: ok = parse_valgrind(line, acc); break;
                case TraceFormat::PIN:      ok = parse_pin(line, acc);      break;
                default: break;
            }

            if (ok) { cb(acc); count++; }
        }
        return count;
    }

    // Generate a synthetic LRU-stress trace for testing
    static void generate_sample(const std::string& path,
                                 uint64_t accesses = 500000,
                                 uint64_t working_set = 1 << 20 /* 1 MB */)
    {
        std::ofstream f(path);
        if (!f.is_open())
            throw std::runtime_error("Cannot create trace: " + path);

        f << "# Cache Simulator Sample Trace\n";
        f << "# Format: R/W address size\n";

        // Phases to test different policies
        std::mt19937_64 rng(12345);

        // Phase 1 (40%): Sequential scan — all policies similar
        uint64_t phase1 = accesses * 40 / 100;
        for (uint64_t i = 0; i < phase1; i++) {
            uint64_t addr = (i * 64) % working_set;
            f << "R 0x" << std::hex << addr << " 8\n";
        }

        // Phase 2 (30%): Small hot loop — LRU excels
        uint64_t phase2 = accesses * 30 / 100;
        uint64_t hot_size = working_set / 16;
        for (uint64_t i = 0; i < phase2; i++) {
            uint64_t addr = (i * 64) % hot_size;
            bool write = (rng() % 5 == 0);
            f << (write ? "W" : "R") << " 0x" << std::hex << addr << " 8\n";
        }

        // Phase 3 (20%): Thrashing — LRU worst case (sequential > assoc)
        uint64_t phase3 = accesses * 20 / 100;
        uint64_t stride  = working_set;
        for (uint64_t i = 0; i < phase3; i++) {
            uint64_t addr = ((i % 8) * stride) % (working_set * 4);
            f << "R 0x" << std::hex << addr << " 8\n";
        }

        // Phase 4 (10%): Random — all policies similar
        uint64_t phase4 = accesses - phase1 - phase2 - phase3;
        for (uint64_t i = 0; i < phase4; i++) {
            uint64_t addr = rng() % working_set;
            f << "R 0x" << std::hex << addr << " 8\n";
        }
    }

private:
    std::string  filepath_;
    TraceFormat  fmt_;

    TraceFormat detect_format(std::ifstream& f) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == 'I' || line[0] == 'L' ||
                line[0] == 'S' || line[0] == 'M') return TraceFormat::VALGRIND;
            if (line[0] == 'R' || line[0] == 'W') {
                // Custom has size field, PIN doesn't always
                std::istringstream ss(line);
                std::string tok; ss >> tok >> tok;
                if (ss >> tok) return TraceFormat::CUSTOM;
                return TraceFormat::PIN;
            }
            break;
        }
        f.seekg(0);
        return TraceFormat::CUSTOM;
    }

    // Format: R/W 0xADDR SIZE
    bool parse_custom(const std::string& line, MemAccess& acc) {
        std::istringstream ss(line);
        char type; std::string addr_str; uint32_t sz = 8;
        if (!(ss >> type >> addr_str)) return false;
        ss >> sz;
        acc.is_write = (type == 'W' || type == 'w');
        acc.addr     = std::stoull(addr_str, nullptr, 16);
        acc.size     = sz;
        return true;
    }

    // Valgrind Lackey format: [I|L|S|M] addr,size
    bool parse_valgrind(const std::string& line, MemAccess& acc) {
        if (line.size() < 3) return false;
        char type = line[0];
        if (type == 'I') return false; // skip instruction fetches
        acc.is_write = (type == 'S'); // M = modify = read+write (treat as write)
        auto comma = line.find(',', 2);
        if (comma == std::string::npos) return false;
        acc.addr = std::stoull(line.substr(2, comma - 2), nullptr, 16);
        acc.size = std::stoul(line.substr(comma + 1));
        return true;
    }

    // PIN format: R/W addr
    bool parse_pin(const std::string& line, MemAccess& acc) {
        std::istringstream ss(line);
        char type; std::string addr_str;
        if (!(ss >> type >> addr_str)) return false;
        acc.is_write = (type == 'W' || type == 'w');
        acc.addr     = std::stoull(addr_str, nullptr, 16);
        acc.size     = 8;
        return true;
    }
};

// Needs <random> for generate_sample
#include <random>
