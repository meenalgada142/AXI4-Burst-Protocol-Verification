#include "coverage_tracker.h"
#include <iostream>
#include <iomanip>

//============================================================
// Sample a transaction (read or write) and bin address/data
// (Unchanged from AXI-Lite version)
//============================================================
void CoverageTracker::sample_transaction(bool is_read, uint32_t addr, uint32_t data) {
    if (is_read) read_count++;
    else         write_count++;

    // Address binning
    if (addr < 0x10)        addr_low++;
    else if (addr < 0x80)   addr_mid++;
    else                    addr_high++;

    // Data pattern binning
    if (data == 0x00000000)      data_zero++;
    else if (data == 0xFFFFFFFF) data_ones++;
    else if ((data & 0xAAAAAAAA) == 0xAAAAAAAA ||
             (data & 0x55555555) == 0x55555555)
        data_alt++;
}

//============================================================
// Sample a completed AXI4 burst write transaction
//============================================================
void CoverageTracker::sample_burst(uint8_t awlen, uint8_t awburst,
                                   uint8_t awid, uint8_t bresp,
                                   bool wlast_correct) {
    //--- Burst length bins ---
    if (awlen == 0)       burst_single++;
    else if (awlen <= 3)  burst_short++;
    else if (awlen <= 15) burst_medium++;
    else                  burst_long++;

    //--- Burst type bins ---
    switch (awburst) {
        case 0: burst_fixed++; break;
        case 1: burst_incr++;  break;
        case 2: burst_wrap++;  break;
        default: break;
    }

    //--- WLAST correctness ---
    if (wlast_correct) wlast_ok++;
    // wlast_early / wlast_missing are set by the monitor
    // when it detects the mismatch mid-burst

    //--- Response bins ---
    if (bresp == 0) bresp_okay++;
    else            bresp_slverr++;

    //--- Per-ID tracking ---
    if (awid < 16) id_bins[awid]++;
}

//============================================================
// Sample FIFO depth (unchanged)
//============================================================
void CoverageTracker::sample_fifo_depth(size_t depth, size_t capacity) {
    if (depth == 0)                                  fifo_empty++;
    else if (depth >= capacity / 2 && depth < capacity) fifo_half++;
    else if (depth == capacity)                      fifo_full++;
}

//============================================================
// Sample error conditions (unchanged)
//============================================================
void CoverageTracker::sample_error(bool invalid_addr, bool corrupt_data) {
    if (invalid_addr) err_invalid_addr++;
    if (corrupt_data) err_corrupt_data++;
}

//============================================================
// Print full coverage report
//============================================================
void CoverageTracker::report() const {
    std::cout << "\n+==========================================+\n";
    std::cout <<   "|         AXI4 Coverage Report             |\n";
    std::cout <<   "+==========================================+\n";

    // Transaction counts
    std::cout << "\n-- Transaction Counts --\n";
    std::cout << "  Reads:  " << read_count << "\n";
    std::cout << "  Writes: " << write_count << "\n";

    // Address bins
    std::cout << "\n-- Address Bins --\n";
    std::cout << "  Low  (<0x10):   " << addr_low  << "\n";
    std::cout << "  Mid  (<0x80):   " << addr_mid  << "\n";
    std::cout << "  High (>=0x80):  " << addr_high << "\n";

    // Data pattern bins
    std::cout << "\n-- Data Pattern Bins --\n";
    std::cout << "  Zero (0x00000000):     " << data_zero << "\n";
    std::cout << "  Ones (0xFFFFFFFF):     " << data_ones << "\n";
    std::cout << "  Alternating (AA/55):   " << data_alt  << "\n";

    // Burst length bins
    std::cout << "\n-- Burst Length Bins --\n";
    std::cout << "  Single (1 beat):       " << burst_single << "\n";
    std::cout << "  Short  (2-4 beats):    " << burst_short  << "\n";
    std::cout << "  Medium (5-16 beats):   " << burst_medium << "\n";
    std::cout << "  Long   (17+ beats):    " << burst_long   << "\n";

    // Burst type bins
    std::cout << "\n-- Burst Type Bins --\n";
    std::cout << "  FIXED: " << burst_fixed << "\n";
    std::cout << "  INCR:  " << burst_incr  << "\n";
    std::cout << "  WRAP:  " << burst_wrap  << "\n";

    // WLAST correctness
    std::cout << "\n-- WLAST Protocol --\n";
    std::cout << "  Correct:  " << wlast_ok      << "\n";
    std::cout << "  Early:    " << wlast_early    << "\n";
    std::cout << "  Missing:  " << wlast_missing  << "\n";

    // Response bins
    std::cout << "\n-- Write Response Bins --\n";
    std::cout << "  OKAY:    " << bresp_okay   << "\n";
    std::cout << "  SLVERR:  " << bresp_slverr << "\n";

    // Per-ID bins
    std::cout << "\n-- Per-ID Transaction Count --\n";
    for (int i = 0; i < 16; i++) {
        if (id_bins[i] > 0)
            std::cout << "  ID " << std::setw(2) << i << ": " << id_bins[i] << "\n";
    }

    // FIFO occupancy
    std::cout << "\n-- FIFO Depth Bins --\n";
    std::cout << "  Empty:     " << fifo_empty << "\n";
    std::cout << "  Half-full: " << fifo_half  << "\n";
    std::cout << "  Full:      " << fifo_full  << "\n";

    // Error bins
    std::cout << "\n-- Error Bins --\n";
    std::cout << "  Invalid address: " << err_invalid_addr << "\n";
    std::cout << "  Corrupt data:    " << err_corrupt_data << "\n";

    std::cout << "\n==========================================\n";
}
