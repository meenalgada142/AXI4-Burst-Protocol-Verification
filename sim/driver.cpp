#include "driver.h"
#include "monitor.h"
#include "globals.h"
#include <iostream>
#include <iomanip>

//============================================================
// Half-clock toggle (one edge)
//============================================================
void AxiLiteDriver::tick() {
    dut->ACLK ^= 1;
    *main_time += 5;
    dut->eval();
    if (tfp) tfp->dump(*main_time);
}

//============================================================
// Advance to the next rising clock edge.
// Samples monitor after eval (NB-settled outputs visible).
//============================================================
static void posedge(AxiLiteDriver* drv) {
    if (drv->dut->ACLK) drv->tick();   // negedge
    drv->tick();                         // posedge
    if (drv->monitor) drv->monitor->sample();
}

//============================================================
// AXI4 Single-Beat Write (AWLEN=0)
//============================================================
void AxiLiteDriver::write(uint32_t addr, uint32_t data, uint8_t id) {
    if (verbose) {
        std::cout << "[WRITE] Addr: 0x" << std::hex << addr
                  << ", Data: 0x" << data << std::dec
                  << ", ID: " << (int)id << std::endl;
    }

    dut->AWADDR  = addr;
    dut->AWID    = id;
    dut->AWLEN   = 0;
    dut->AWSIZE  = 2;
    dut->AWBURST = 1;
    dut->AWVALID = 1;

    dut->WDATA   = data;
    dut->WSTRB   = 0xF;
    dut->WLAST   = 1;
    dut->WVALID  = 1;
    dut->eval();

    // Address phase
    int timeout = 200;
    while (timeout-- > 0) {
        posedge(this);
        if (dut->AWREADY) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Timeout waiting for AWREADY\n";
        dut->AWVALID = 0; dut->WVALID = 0; dut->WLAST = 0;
        return;
    }
    dut->AWVALID = 0;
    dut->eval();

    // Data phase
    timeout = 200;
    while (timeout-- > 0) {
        bool wready_old = dut->WREADY;
        posedge(this);
        if (wready_old) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Timeout waiting for WREADY\n";
        dut->WVALID = 0; dut->WLAST = 0;
        return;
    }
    dut->WVALID = 0;
    dut->WLAST  = 0;
    dut->eval();

    // Response phase
    timeout = 200;
    while (timeout-- > 0) {
        posedge(this);
        if (dut->BVALID) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Timeout waiting for BVALID\n";
        return;
    }
    dut->BREADY = 1;
    dut->eval();
    posedge(this);
    dut->BREADY = 0;
    dut->eval();

    if (dut->BRESP != 0)
        std::cerr << "[ERROR] BRESP = " << (int)dut->BRESP << "\n";
}

//============================================================
// AXI4 Single-Beat Read (ARLEN=0)
//============================================================
// Now drives full AXI4 AR channel signals and checks RID/RLAST.
//============================================================
uint32_t AxiLiteDriver::read(uint32_t addr, uint8_t id) {
    if (verbose) {
        std::cout << "[READ] Addr: 0x" << std::hex << addr
                  << std::dec << ", ID: " << (int)id << std::endl;
    }

    dut->ARADDR   = addr;
    dut->ARID     = id;
    dut->ARLEN    = 0;       // single beat
    dut->ARSIZE   = 2;       // 4 bytes
    dut->ARBURST  = 1;       // INCR (irrelevant for single beat)
    dut->ARVALID  = 1;
    dut->eval();

    // Address phase: wait for ARREADY
    int timeout = 200;
    while (timeout-- > 0) {
        posedge(this);
        if (dut->ARREADY) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Timeout waiting for ARREADY\n";
        dut->ARVALID = 0;
        return 0xFFFFFFFF;
    }
    dut->ARVALID = 0;
    dut->eval();

    // Data phase: wait for RVALID
    timeout = 200;
    while (timeout-- > 0) {
        posedge(this);
        if (dut->RVALID) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Timeout waiting for RVALID\n";
        return 0xFFFFFFFF;
    }

    uint32_t val  = dut->RDATA;
    uint8_t  resp = dut->RRESP;
    uint8_t  rid  = dut->RID;
    uint8_t  rlast = dut->RLAST;

    // Accept beat
    dut->RREADY = 1;
    dut->eval();
    posedge(this);
    dut->RREADY = 0;
    dut->eval();

    // Protocol checks
    if (resp != 0)
        std::cerr << "[ERROR] RRESP = " << (int)resp << "\n";
    if (rid != id)
        std::cerr << "[ERROR] RID mismatch: expected " << (int)id
                  << ", got " << (int)rid << "\n";
    if (!rlast)
        std::cerr << "[ERROR] RLAST not asserted on single-beat read\n";

    return val;
}

//============================================================
// AXI4 Burst Write (unchanged from previous version)
//============================================================
void AxiLiteDriver::axi4_burst_write(uint32_t base_addr,
                                      const std::vector<uint32_t>& data,
                                      uint8_t burst_type,
                                      uint8_t size,
                                      uint8_t id) {
    if (data.empty()) return;
    uint8_t awlen = (uint8_t)(data.size() - 1);

    if (verbose) {
        std::cout << "[BURST WRITE] Addr: 0x" << std::hex << base_addr
                  << ", Beats: " << std::dec << data.size()
                  << ", Type: " << (int)burst_type
                  << ", ID: " << (int)id << std::endl;
    }

    dut->AWADDR  = base_addr;
    dut->AWID    = id;
    dut->AWLEN   = awlen;
    dut->AWSIZE  = size;
    dut->AWBURST = burst_type;
    dut->AWVALID = 1;
    dut->WVALID  = 0;
    dut->eval();

    int timeout = 200;
    while (timeout-- > 0) { posedge(this); if (dut->AWREADY) break; }
    if (timeout <= 0) { std::cerr << "[ERROR] Burst: Timeout AWREADY\n"; dut->AWVALID=0; return; }
    dut->AWVALID = 0;
    dut->eval();

    for (size_t beat = 0; beat < data.size(); beat++) {
        dut->WDATA  = data[beat];
        dut->WSTRB  = 0xF;
        dut->WLAST  = (beat == data.size() - 1) ? 1 : 0;
        dut->WVALID = 1;
        dut->eval();

        timeout = 200;
        while (timeout-- > 0) {
            bool wready_old = dut->WREADY;
            posedge(this);
            if (wready_old) break;
        }
        if (timeout <= 0) {
            std::cerr << "[ERROR] Burst: Timeout WREADY beat " << beat << "\n";
            dut->WVALID = 0; dut->WLAST = 0; return;
        }
        if (verbose)
            std::cout << "  W Beat " << beat << ": 0x" << std::hex
                      << data[beat] << std::dec
                      << ((beat == data.size()-1) ? " [LAST]" : "") << "\n";
    }
    dut->WVALID = 0; dut->WLAST = 0; dut->eval();

    timeout = 200;
    while (timeout-- > 0) { posedge(this); if (dut->BVALID) break; }
    if (timeout <= 0) { std::cerr << "[ERROR] Burst: Timeout BVALID\n"; return; }

    uint8_t resp = dut->BRESP;
    uint8_t rid  = dut->BID;
    dut->BREADY = 1; dut->eval();
    posedge(this);
    dut->BREADY = 0; dut->eval();

    if (verbose)
        std::cout << "[BURST RESP] BID: " << (int)rid << ", BRESP: " << (int)resp << "\n";
    if (resp != 0) std::cerr << "[ERROR] Burst BRESP = " << (int)resp << "\n";
    if (rid != id) std::cerr << "[ERROR] BID mismatch\n";
}

//============================================================
// AXI4 Burst Read
//============================================================
// Protocol:
//   1. AR phase: drive ARID/ARADDR/ARLEN/ARSIZE/ARBURST/ARVALID
//      → wait for ARREADY handshake → deassert ARVALID
//   2. R phase: for each beat, wait for RVALID, assert RREADY,
//      capture RDATA/RID/RLAST, verify RID, check RLAST on
//      final beat.
//============================================================
std::vector<uint32_t> AxiLiteDriver::axi4_burst_read(uint32_t base_addr,
                                                       uint8_t  num_beats,
                                                       uint8_t  burst_type,
                                                       uint8_t  size,
                                                       uint8_t  id) {
    std::vector<uint32_t> results;
    if (num_beats == 0) return results;
    uint8_t arlen = num_beats - 1;

    if (verbose) {
        std::cout << "[BURST READ] Addr: 0x" << std::hex << base_addr
                  << ", Beats: " << std::dec << (int)num_beats
                  << ", Type: " << (int)burst_type
                  << ", ID: " << (int)id << std::endl;
    }

    // ── AR phase ──
    dut->ARADDR   = base_addr;
    dut->ARID     = id;
    dut->ARLEN    = arlen;
    dut->ARSIZE   = size;
    dut->ARBURST  = burst_type;
    dut->ARVALID  = 1;
    dut->RREADY   = 0;
    dut->eval();

    int timeout = 200;
    while (timeout-- > 0) {
        posedge(this);
        if (dut->ARREADY) break;
    }
    if (timeout <= 0) {
        std::cerr << "[ERROR] Burst read: Timeout ARREADY\n";
        dut->ARVALID = 0;
        return results;
    }
    dut->ARVALID = 0;
    dut->eval();

    // ── R phase: collect beats ──
    for (uint8_t beat = 0; beat < num_beats; beat++) {
        // Wait for RVALID
        timeout = 200;
        while (timeout-- > 0) {
            posedge(this);
            if (dut->RVALID) break;
        }
        if (timeout <= 0) {
            std::cerr << "[ERROR] Burst read: Timeout RVALID at beat "
                      << (int)beat << "\n";
            return results;
        }

        // Capture data before accepting
        uint32_t val   = dut->RDATA;
        uint8_t  resp  = dut->RRESP;
        uint8_t  rid   = dut->RID;
        uint8_t  rlast = dut->RLAST;

        // Accept: assert RREADY for one posedge
        dut->RREADY = 1;
        dut->eval();
        posedge(this);
        dut->RREADY = 0;
        dut->eval();

        results.push_back(val);

        if (verbose) {
            std::cout << "  R Beat " << (int)beat << ": 0x" << std::hex
                      << val << std::dec
                      << " RID=" << (int)rid
                      << " RLAST=" << (int)rlast
                      << ((beat == num_beats - 1) ? " [FINAL]" : "")
                      << "\n";
        }

        // Protocol checks
        if (rid != id)
            std::cerr << "[ERROR] RID mismatch at beat " << (int)beat
                      << ": expected " << (int)id << ", got " << (int)rid << "\n";
        if (resp != 0)
            std::cerr << "[ERROR] RRESP = " << (int)resp << " at beat "
                      << (int)beat << "\n";

        // RLAST check
        bool should_be_last = (beat == num_beats - 1);
        if (rlast && !should_be_last)
            std::cerr << "[ERROR] RLAST asserted early at beat "
                      << (int)beat << " of " << (int)(num_beats-1) << "\n";
        if (!rlast && should_be_last)
            std::cerr << "[ERROR] RLAST not asserted on final beat "
                      << (int)beat << "\n";

        // If RLAST came early, stop
        if (rlast && !should_be_last) {
            std::cerr << "[WARN] Burst terminated early by RLAST\n";
            break;
        }
    }

    return results;
}

//============================================================
// Bad WLAST burst write (unchanged from previous version)
//============================================================
void AxiLiteDriver::axi4_burst_write_bad_wlast(uint32_t base_addr,
                                                 const std::vector<uint32_t>& data,
                                                 bool early_last,
                                                 uint8_t burst_type,
                                                 uint8_t size,
                                                 uint8_t id) {
    if (data.size() < 2) return;
    uint8_t awlen = (uint8_t)(data.size() - 1);
    if (verbose)
        std::cout << "[BAD WLAST] Addr: 0x" << std::hex << base_addr
                  << ", Beats: " << std::dec << data.size()
                  << ", Error: " << (early_last ? "EARLY" : "MISSING") << "\n";

    dut->AWADDR = base_addr; dut->AWID = id; dut->AWLEN = awlen;
    dut->AWSIZE = size; dut->AWBURST = burst_type; dut->AWVALID = 1;
    dut->WVALID = 0; dut->eval();

    int timeout = 200;
    while (timeout-- > 0) { posedge(this); if (dut->AWREADY) break; }
    if (timeout <= 0) { dut->AWVALID = 0; return; }
    dut->AWVALID = 0; dut->eval();

    for (size_t beat = 0; beat < data.size(); beat++) {
        dut->WDATA = data[beat]; dut->WSTRB = 0xF; dut->WVALID = 1;
        if (early_last) dut->WLAST = (beat == data.size()-2) ? 1 : 0;
        else            dut->WLAST = 0;
        dut->eval();
        timeout = 200;
        while (timeout-- > 0) { bool wo = dut->WREADY; posedge(this); if (wo) break; }
        if (timeout <= 0) break;
        if (!dut->WREADY && beat < data.size()-1) break;
    }
    dut->WVALID = 0; dut->WLAST = 0; dut->eval();

    timeout = 200;
    while (timeout-- > 0) { posedge(this); if (dut->BVALID) break; }
    if (timeout <= 0) return;
    dut->BREADY = 1; dut->eval(); posedge(this); dut->BREADY = 0; dut->eval();
}

//============================================================
// Legacy burst helpers
//============================================================
void AxiLiteDriver::burst_write(uint32_t base_addr, const std::vector<uint32_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) write(base_addr + i*4, data[i]);
}
std::vector<uint32_t> AxiLiteDriver::burst_read(uint32_t base_addr, size_t count) {
    std::vector<uint32_t> r;
    for (size_t i = 0; i < count; ++i) r.push_back(read(base_addr + i*4));
    return r;
}

//============================================================
// Error injection
//============================================================
void AxiLiteDriver::inject_error(uint32_t addr, uint32_t data, bool corrupt_data, bool invalid_addr) {
    if (invalid_addr) addr = 0xDEADBEEF;
    if (corrupt_data) data ^= 0xFFFFFFFF;
    write(addr, data);
}
