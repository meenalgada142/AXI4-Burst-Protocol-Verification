#ifndef SIM_UTILS_H
#define SIM_UTILS_H

#include "Vdut.h"
#include "verilated_vcd_c.h"

// Clock tick: toggles ACLK, advances time, evaluates, dumps waveform
void tick(Vdut* dut, VerilatedVcdC* tfp);

// Debug: print all handshake signal states
void log_handshakes(Vdut* dut);

#endif // SIM_UTILS_H
