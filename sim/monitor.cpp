#include "monitor.h"
#include <iostream>
#include <iomanip>

void AxiLiteMonitor::sample() {

    //============================================================
    // AW handshake: latch write address phase
    //============================================================
    if (dut->AWVALID && dut->AWREADY) {
        latched_awaddr  = dut->AWADDR;
        latched_awlen   = dut->AWLEN;
        latched_awburst = dut->AWBURST;
        latched_awid    = dut->AWID;
        aw_phase_seen   = true;
        beat_count      = 0;
        wlast_error_detected = false;
    }

    //============================================================
    // W beat acceptance: WVALID × prev_WREADY
    //============================================================
    if (dut->WVALID && prev_wready && aw_phase_seen) {
        bool is_final = (beat_count == latched_awlen);
        if (dut->WLAST && !is_final) {
            if (!wlast_error_detected) {
                std::cout << "[MONITOR @ " << *main_time
                          << "ns] WLAST ERROR: early on beat "
                          << (int)beat_count << " of " << (int)latched_awlen << "\n";
                wlast_error_detected = true;
                if (coverage) coverage->wlast_early++;
            }
        } else if (!dut->WLAST && is_final) {
            if (!wlast_error_detected) {
                std::cout << "[MONITOR @ " << *main_time
                          << "ns] WLAST ERROR: missing on final beat "
                          << (int)beat_count << "\n";
                wlast_error_detected = true;
                if (coverage) coverage->wlast_missing++;
            }
        }
        if (coverage)
            coverage->sample_transaction(false,
                latched_awaddr + beat_count * 4, dut->WDATA);
        beat_count++;
    }

    //============================================================
    // B response: prev_BVALID × BREADY
    //============================================================
    if (prev_bvalid && dut->BREADY) {
        AxiWriteResp wr;
        wr.addr = latched_awaddr; wr.data = dut->WDATA;
        wr.resp = dut->BRESP;     wr.id   = dut->BID;
        wr.awlen = latched_awlen; wr.awburst = latched_awburst;
        write_log.push_back(wr);
        std::cout << "[MONITOR @ " << *main_time << "ns] WriteResp:"
                  << " addr=0x" << std::hex << wr.addr
                  << ", BID=" << std::dec << (int)wr.id
                  << ", BRESP=" << (int)wr.resp
                  << ", AWLEN=" << (int)wr.awlen
                  << (wr.awlen > 0 ? " (BURST)" : "") << "\n";
        if (coverage) {
            coverage->sample_burst(latched_awlen, latched_awburst,
                                   wr.id, wr.resp, !wlast_error_detected);
            if (wr.resp == 2) coverage->sample_error(false, true);
        }
        aw_phase_seen = false;
    }

    //============================================================
    // AR handshake: latch read address phase
    //============================================================
    if (dut->ARVALID && dut->ARREADY) {
        latched_araddr  = dut->ARADDR;
        latched_arlen   = dut->ARLEN;
        latched_arburst = dut->ARBURST;
        latched_arid    = dut->ARID;
        ar_phase_seen   = true;
        rd_beat_count   = 0;
        rlast_error_detected = false;
    }

    //============================================================
    // R beat acceptance: prev_RVALID × RREADY
    //============================================================
    if (prev_rvalid && dut->RREADY && ar_phase_seen) {
        AxiReadResp rd;
        rd.addr  = latched_araddr;
        rd.data  = dut->RDATA;
        rd.resp  = dut->RRESP;
        rd.id    = dut->RID;
        rd.rlast = dut->RLAST;
        read_log.push_back(rd);

        bool is_final = (rd_beat_count == latched_arlen);

        // ── RLAST validation ──
        if (dut->RLAST && !is_final) {
            if (!rlast_error_detected) {
                std::cout << "[MONITOR @ " << *main_time
                          << "ns] RLAST ERROR: early on beat "
                          << (int)rd_beat_count << " of "
                          << (int)latched_arlen << "\n";
                rlast_error_detected = true;
            }
        } else if (!dut->RLAST && is_final) {
            if (!rlast_error_detected) {
                std::cout << "[MONITOR @ " << *main_time
                          << "ns] RLAST ERROR: missing on final beat "
                          << (int)rd_beat_count << "\n";
                rlast_error_detected = true;
            }
        }

        // ── RID check ──
        if (dut->RID != latched_arid) {
            std::cout << "[MONITOR @ " << *main_time
                      << "ns] RID MISMATCH: expected "
                      << (int)latched_arid << ", got " << (int)dut->RID << "\n";
        }

        if (coverage)
            coverage->sample_transaction(true,
                latched_araddr + rd_beat_count * 4, dut->RDATA);

        rd_beat_count++;

        // Burst complete?
        if (dut->RLAST || is_final) {
            ar_phase_seen = false;
        }
    }

    //============================================================
    // FIFO depth sampling
    //============================================================
    if (coverage) {
        if (dut->AWVALID || dut->WVALID || dut->ARVALID ||
            dut->RVALID  || dut->BVALID)
            coverage->sample_fifo_depth(dut->fifo_level, 16);
    }

    //============================================================
    // Update previous-cycle values
    //============================================================
    prev_wready = dut->WREADY;
    prev_bvalid = dut->BVALID;
    prev_rvalid = dut->RVALID;
}

void AxiLiteMonitor::print_logs() {
    std::cout << "\n=== AXI4 Write Log ===\n";
    for (const auto& wr : write_log)
        std::cout << "  addr=0x" << std::hex << wr.addr
                  << ", ID=" << std::dec << (int)wr.id
                  << ", BRESP=" << (int)wr.resp
                  << ", AWLEN=" << (int)wr.awlen << "\n";

    std::cout << "\n=== AXI4 Read Log ===\n";
    for (const auto& rd : read_log)
        std::cout << "  addr=0x" << std::hex << rd.addr
                  << ", data=0x" << rd.data
                  << ", RID=" << std::dec << (int)rd.id
                  << ", RRESP=" << (int)rd.resp
                  << ", RLAST=" << (int)rd.rlast << "\n";
}
