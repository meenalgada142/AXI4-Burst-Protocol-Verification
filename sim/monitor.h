#ifndef MONITOR_H
#define MONITOR_H

#include "Vdut.h"
#include "coverage_tracker.h"
#include <vector>
#include <cstdint>

struct AxiWriteResp {
    uint32_t addr;
    uint32_t data;
    uint8_t  resp;
    uint8_t  id;
    uint8_t  awlen;
    uint8_t  awburst;
};

struct AxiReadResp {
    uint32_t addr;
    uint32_t data;
    uint8_t  resp;
    uint8_t  id;
    uint8_t  rlast;
};

class AxiLiteMonitor {
public:
    Vdut*            dut;
    vluint64_t*      main_time;
    CoverageTracker* coverage;

    std::vector<AxiWriteResp> write_log;
    std::vector<AxiReadResp>  read_log;

    // ── Write burst tracking ──
    uint32_t latched_awaddr  = 0;
    uint8_t  latched_awlen   = 0;
    uint8_t  latched_awburst = 0;
    uint8_t  latched_awid    = 0;
    bool     aw_phase_seen   = false;
    uint8_t  beat_count      = 0;
    bool     wlast_error_detected = false;

    // ── Read burst tracking ──
    uint32_t latched_araddr  = 0;
    uint8_t  latched_arlen   = 0;
    uint8_t  latched_arburst = 0;
    uint8_t  latched_arid    = 0;
    bool     ar_phase_seen   = false;
    uint8_t  rd_beat_count   = 0;
    bool     rlast_error_detected = false;

    // ── Previous-cycle registered outputs ──
    uint8_t  prev_wready  = 0;
    uint8_t  prev_bvalid  = 0;
    uint8_t  prev_rvalid  = 0;

    AxiLiteMonitor(Vdut* d, vluint64_t* t, CoverageTracker* c)
        : dut(d), main_time(t), coverage(c) {}

    void sample();
    void print_logs();
};

#endif
