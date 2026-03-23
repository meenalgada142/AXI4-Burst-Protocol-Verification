#ifndef GLOBALS_H
#define GLOBALS_H

#include "verilated.h"
#include "verilated_vcd_c.h"

// Forward declarations
class Vdut;
class AxiLiteMonitor;

// Global simulation state (defined in tb_top.cpp)
extern vluint64_t main_time;
extern Vdut*           dut;
extern VerilatedVcdC*  tfp;
extern AxiLiteMonitor* monitor;

#endif // GLOBALS_H
