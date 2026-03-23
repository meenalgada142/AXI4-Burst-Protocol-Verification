#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>

//============================================================
// CoverageTracker
// Tracks functional coverage for AXI-Lite FIFO transactions
//============================================================
class CoverageTracker {
public:
    //=======================
    // Sampling methods
    //=======================

    // Track a read or write transaction with address and data
    void sample_transaction(bool is_read, uint32_t addr, uint32_t data);

    // Track FIFO occupancy level
    void sample_fifo_depth(std::size_t depth, std::size_t capacity);

    // Track protocol errors (e.g., invalid address, data corruption)
    void sample_error(bool invalid_addr, bool corrupt_data);

    //=======================
    // Reporting method
    //=======================

    // Print a full coverage report to stdout
    void report() const;

private:
    //=======================
    // Transaction counters
    //=======================
    std::size_t read_count  = 0;  // Total read operations
    std::size_t write_count = 0;  // Total write operations

    //=======================
    // Address bins
    //=======================
    std::size_t addr_low  = 0;  // Address < 0x10
    std::size_t addr_mid  = 0;  // 0x10 <= Address < 0x80
    std::size_t addr_high = 0;  // Address >= 0x80

    //=======================
    // Data pattern bins
    //=======================
    std::size_t data_zero = 0;  // Data == 0x00000000
    std::size_t data_ones = 0;  // Data == 0xFFFFFFFF
    std::size_t data_alt  = 0;  // Alternating pattern (0xAA/0x55)

    //=======================
    // FIFO depth bins
    //=======================
    std::size_t fifo_empty = 0;  // FIFO depth == 0
    std::size_t fifo_half  = 0;  // FIFO depth >= half but < full
    std::size_t fifo_full  = 0;  // FIFO depth == capacity

    //=======================
    // Error bins
    //=======================
    std::size_t err_invalid_addr = 0;  // Invalid address access
    std::size_t err_corrupt_data = 0;  // Data corruption detected
};
