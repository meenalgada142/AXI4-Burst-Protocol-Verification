#pragma once

#include <vector>
#include <cstdint>
#include "Vdut.h"
#include "verilated_vcd_c.h"

//============================================================
// AxiLiteDriver
// Provides read/write access to AXI-Lite DUT with waveform tracing
//============================================================
class AxiLiteDriver {
public:
    //=======================
    // DUT and simulation handles
    //=======================
    Vdut* dut;                    // Pointer to Verilated DUT instance
    vluint64_t* main_time;       // Simulation time tracker
    VerilatedVcdC* tfp;          // Optional waveform tracer
    bool verbose;                // Enable verbose logging

    //=======================
    // Constructor
    //=======================
    AxiLiteDriver(Vdut* d, vluint64_t* t, VerilatedVcdC* tracer = nullptr)
        : dut(d), main_time(t), tfp(tracer), verbose(false) {}

    //=======================
    // Simulation control
    //=======================
    void tick();  // Advance simulation by one clock edge

    //=======================
    // AXI-Lite transactions
    //=======================
    void write(uint32_t addr, uint32_t data);       // Single write
    uint32_t read(uint32_t addr);                   // Single read

    //=======================
    // Burst transactions
    //=======================
    void burst_write(uint32_t base_addr, const std::vector<uint32_t>& data);  // Multiple writes
    std::vector<uint32_t> burst_read(uint32_t base_addr, size_t count);       // Multiple reads

    //=======================
    // Error injection
    //=======================
    void inject_error(uint32_t addr, uint32_t data, bool corrupt_data, bool invalid_addr);  // Protocol violation
};
