#pragma once

#include "Vdut.h"                 // Verilated DUT model
#include "coverage_tracker.h"    // Coverage tracking interface
#include <vector>
#include <cstdint>

//============================================================
// AXI-Lite Write Response Struct
// Captures address, data, and BRESP for each write transaction
//============================================================
struct AxiWriteResp {
    uint32_t addr;   // Write address
    uint32_t data;   // Write data
    uint8_t  resp;   // Write response (BRESP)
};

//============================================================
// AXI-Lite Read Response Struct
// Captures address, data, and RRESP for each read transaction
//============================================================
struct AxiReadResp {
    uint32_t addr;   // Read address
    uint32_t data;   // Read data
    uint8_t  resp;   // Read response (RRESP)
};

//============================================================
// AxiLiteMonitor
// Observes DUT signals and logs AXI-Lite transactions
//============================================================
class AxiLiteMonitor {
public:
    /**
     * @brief Construct a monitor with DUT, simulation time, and coverage tracker.
     * @param dut Pointer to Verilated DUT instance
     * @param time Pointer to simulation time variable
     * @param cov Pointer to coverage tracker instance
     */
    AxiLiteMonitor(Vdut* dut, vluint64_t* time, CoverageTracker* cov)
        : dut(dut), main_time(time), coverage(cov) {}

    /**
     * @brief Sample DUT signals to detect completed AXI-Lite transactions.
     * Logs write/read responses and updates coverage bins.
     */
    void sample();

    /**
     * @brief Print all logged AXI-Lite write and read transactions.
     */
    void print_logs();

private:
    Vdut* dut;                      // Verilated DUT instance
    vluint64_t* main_time;         // Simulation time pointer
    CoverageTracker* coverage;     // Coverage tracker interface

    std::vector<AxiWriteResp> write_log;  // Logged write responses
    std::vector<AxiReadResp>  read_log;   // Logged read responses
};
