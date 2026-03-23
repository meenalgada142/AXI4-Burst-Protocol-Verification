#include "Vdut.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <iostream>
#include "driver.h"
#include "monitor.h"
#include "coverage_tracker.h"
#include "sim_utils.h"
#include "globals.h"

extern void run_fifo_sequence(AxiLiteDriver&);

vluint64_t main_time = 0;
double sc_time_stamp() { return main_time; }

Vdut*           dut     = nullptr;
VerilatedVcdC*  tfp     = nullptr;
AxiLiteMonitor* monitor = nullptr;

void tick(Vdut* dut, VerilatedVcdC* tfp) {
    dut->ACLK ^= 1;
    main_time += 5;
    dut->eval();
    if (tfp) tfp->dump(main_time);
}

void log_handshakes(Vdut* dut) {
    std::cout << "AW: V=" << (int)dut->AWVALID << " R=" << (int)dut->AWREADY
              << " ID=" << (int)dut->AWID << " LEN=" << (int)dut->AWLEN
              << " | W: V=" << (int)dut->WVALID << " R=" << (int)dut->WREADY
              << " LAST=" << (int)dut->WLAST
              << " | B: V=" << (int)dut->BVALID << " R=" << (int)dut->BREADY
              << " ID=" << (int)dut->BID << " RESP=" << (int)dut->BRESP
              << std::endl;
    std::cout << "AR: V=" << (int)dut->ARVALID << " R=" << (int)dut->ARREADY
              << " ID=" << (int)dut->ARID << " LEN=" << (int)dut->ARLEN
              << " | R: V=" << (int)dut->RVALID << " R=" << (int)dut->RREADY
              << " ID=" << (int)dut->RID << " LAST=" << (int)dut->RLAST
              << std::endl;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    dut = new Vdut;
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("wave.vcd");

    // ── Initial signal state ──
    dut->ACLK    = 0;
    dut->ARESETn = 0;

    // Write address channel
    dut->AWID    = 0;
    dut->AWADDR  = 0;
    dut->AWLEN   = 0;
    dut->AWSIZE  = 2;
    dut->AWBURST = 1;
    dut->AWVALID = 0;

    // Write data channel
    dut->WDATA   = 0;
    dut->WSTRB   = 0xF;
    dut->WLAST   = 0;
    dut->WVALID  = 0;

    // Write response channel
    dut->BREADY  = 0;

    // Read address channel (NEW — full AXI4)
    dut->ARID    = 0;
    dut->ARADDR  = 0;
    dut->ARLEN   = 0;
    dut->ARSIZE  = 2;
    dut->ARBURST = 1;
    dut->ARVALID = 0;

    // Read data channel
    dut->RREADY  = 0;

    // ── Construct TB components ──
    CoverageTracker coverage;
    AxiLiteDriver driver(dut, &main_time, tfp);
    driver.verbose = true;
    monitor = new AxiLiteMonitor(dut, &main_time, &coverage);
    driver.monitor = monitor;

    // ── Reset ──
    std::cout << "[INFO] Applying reset..." << std::endl;
    for (int i = 0; i < 50; ++i) tick(dut, tfp);
    dut->ARESETn = 1;
    tick(dut, tfp);
    for (int i = 0; i < 5; ++i) tick(dut, tfp);
    std::cout << "[INFO] Reset released." << std::endl;

    // ── Run tests ──
    std::cout << "[INFO] Running AXI4 burst test sequence..." << std::endl;
    run_fifo_sequence(driver);

    // ── Reports ──
    std::cout << "\n[INFO] Test sequence complete.\n";
    monitor->print_logs();
    coverage.report();

    // ── Cleanup ──
    dut->final();
    tfp->close();
    delete monitor;
    delete tfp;
    delete dut;
    return 0;
}
