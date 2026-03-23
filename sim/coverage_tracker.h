#ifndef COVERAGE_TRACKER_H
#define COVERAGE_TRACKER_H

#include <cstddef>
#include <cstdint>

class CoverageTracker {
public:
    //--- Original AXI-Lite bins ---
    int read_count  = 0, write_count = 0;
    int addr_low    = 0, addr_mid    = 0, addr_high = 0;
    int data_zero   = 0, data_ones   = 0, data_alt  = 0;
    int fifo_empty  = 0, fifo_half   = 0, fifo_full = 0;
    int err_invalid_addr = 0, err_corrupt_data = 0;

    //--- AXI4 burst-specific bins ---
    int burst_single   = 0;   // AWLEN == 0  (1 beat)
    int burst_short    = 0;   // AWLEN 1..3  (2-4 beats)
    int burst_medium   = 0;   // AWLEN 4..15 (5-16 beats)
    int burst_long     = 0;   // AWLEN > 15  (17+ beats)

    int burst_fixed    = 0;   // AWBURST == 00
    int burst_incr     = 0;   // AWBURST == 01
    int burst_wrap     = 0;   // AWBURST == 10

    int wlast_ok       = 0;   // WLAST correct on final beat
    int wlast_early    = 0;   // WLAST asserted too early
    int wlast_missing  = 0;   // WLAST not asserted on final beat

    int bresp_okay     = 0;   // BRESP == 0
    int bresp_slverr   = 0;   // BRESP == 2

    int id_bins[16]    = {};   // Per-ID transaction count (4-bit ID)

    // Sample a single-beat or legacy transaction
    void sample_transaction(bool is_read, uint32_t addr, uint32_t data);

    // Sample a completed AXI4 burst write transaction
    void sample_burst(uint8_t awlen, uint8_t awburst, uint8_t awid,
                      uint8_t bresp, bool wlast_correct);

    // Sample FIFO depth
    void sample_fifo_depth(size_t depth, size_t capacity);

    // Sample error conditions
    void sample_error(bool invalid_addr, bool corrupt_data);

    // Print coverage report
    void report() const;
};

#endif // COVERAGE_TRACKER_H
