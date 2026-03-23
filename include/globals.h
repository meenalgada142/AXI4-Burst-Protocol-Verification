#pragma once

#include "Vdut.h"               // Verilated DUT model
#include "verilated_vcd_c.h"    // VCD waveform tracing
#include "monitor.h"            // Coverage and protocol monitor

//============================================================
// Global simulation handles
// These are shared across driver, monitor, and testbench
//============================================================

// DUT instance (Verilated model of top-level module)
extern Vdut* dut;

// Waveform tracer (optional, used for dumping .vcd files)
extern VerilatedVcdC* tfp;

// Protocol monitor (used for coverage sampling and checks)
extern AxiLiteMonitor* monitor;
