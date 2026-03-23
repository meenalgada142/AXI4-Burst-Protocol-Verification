#ifndef DRIVER_H
#define DRIVER_H

#include "Vdut.h"
#include "verilated_vcd_c.h"
#include <cstdint>
#include <vector>

class AxiLiteMonitor;

class AxiLiteDriver {
public:
    Vdut*           dut;
    vluint64_t*     main_time;
    VerilatedVcdC*  tfp;
    AxiLiteMonitor* monitor = nullptr;
    bool            verbose = false;

    AxiLiteDriver(Vdut* d, vluint64_t* t, VerilatedVcdC* f)
        : dut(d), main_time(t), tfp(f) {}

    void tick();

    //--- Single-beat write (AWLEN=0) ---
    void     write(uint32_t addr, uint32_t data, uint8_t id = 0);

    //--- Single-beat read (ARLEN=0, backward compatible) ---
    uint32_t read(uint32_t addr, uint8_t id = 0);

    //--- AXI4 burst write ---
    void axi4_burst_write(uint32_t base_addr,
                          const std::vector<uint32_t>& data,
                          uint8_t burst_type = 1,
                          uint8_t size = 2,
                          uint8_t id = 0);

    //--- AXI4 burst read ---
    //   Returns a vector of read-back data, one entry per beat.
    //   burst_type: 0=FIXED, 1=INCR, 2=WRAP
    //   size: bytes per beat as log2 (2 = 4 bytes for 32-bit)
    std::vector<uint32_t> axi4_burst_read(uint32_t base_addr,
                                           uint8_t  num_beats,
                                           uint8_t  burst_type = 1,
                                           uint8_t  size = 2,
                                           uint8_t  id = 0);

    //--- Bad WLAST burst write (error injection) ---
    void axi4_burst_write_bad_wlast(uint32_t base_addr,
                                    const std::vector<uint32_t>& data,
                                    bool early_last,
                                    uint8_t burst_type = 1,
                                    uint8_t size = 2,
                                    uint8_t id = 0);

    //--- Legacy software burst ---
    void burst_write(uint32_t base_addr, const std::vector<uint32_t>& data);
    std::vector<uint32_t> burst_read(uint32_t base_addr, size_t count);

    //--- Error injection ---
    void inject_error(uint32_t addr, uint32_t data,
                      bool corrupt_data = false, bool invalid_addr = false);
};

#endif
