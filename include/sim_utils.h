#pragma once

#include "Vdut.h"               // Verilated DUT model
#include "verilated_vcd_c.h"   // VCD tracing support

/**
 * @brief Advance simulation by one clock edge and dump waveform if enabled.
 * 
 * @param dut Pointer to the Verilated DUT instance
 * @param tfp Pointer to the waveform tracer (can be nullptr if tracing is disabled)
 */
void tick(Vdut* dut, VerilatedVcdC* tfp);

/**
 * @brief Log AXI-Lite handshake signals for debugging (optional utility).
 * 
 * @param dut Pointer to the Verilated DUT instance
 */
void log_handshakes(Vdut* dut);
