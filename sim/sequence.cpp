#include "driver.h"
#include "monitor.h"
#include "coverage_tracker.h"
#include "Vdut.h"
#include "sim_utils.h"
#include "globals.h"
#include <iostream>
#include <iomanip>
#include <vector>

extern vluint64_t     main_time;
extern VerilatedVcdC* tfp;
extern Vdut*          dut;

void tick_and_sample() {
    monitor->sample();
    tick(dut, tfp);
    monitor->sample();
}

//============================================================
// Test 1: Single-beat FIFO writes & reads
//============================================================
static void test_single_beat_fifo(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 1: Single-beat FIFO writes & reads\n";
    std::cout << "────────────────────────────────────────\n";

    driver.write(0x00, 0xA1);  tick_and_sample(); tick_and_sample();
    driver.write(0x00, 0xB2);  tick_and_sample(); tick_and_sample();
    driver.write(0x00, 0xC3);  tick_and_sample(); tick_and_sample();
    driver.write(0x00, 0xD4);  tick_and_sample(); tick_and_sample();

    for (int i = 0; i < 4; i++) {
        uint32_t val = driver.read(0x10);
        tick_and_sample(); tick_and_sample();
        std::cout << "  FIFO pop[" << i << "]: 0x" << std::hex << val << std::dec << "\n";
    }

    uint32_t status = driver.read(0x20);
    tick_and_sample(); tick_and_sample();
    std::cout << "  FIFO status: full=" << ((status >> 1) & 1)
              << ", empty=" << (status & 1) << "\n";
}

//============================================================
// Test 2: Write 4-beat INCR burst + single-beat read-back
//============================================================
static void test_incr_write_and_readback(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 2: INCR burst write + single-beat reads\n";
    std::cout << "────────────────────────────────────────\n";

    std::vector<uint32_t> data = {0xCAFE0001, 0xCAFE0002, 0xCAFE0003, 0xCAFE0004};
    driver.axi4_burst_write(0x100, data, 1, 2, 1);
    tick_and_sample(); tick_and_sample();

    std::cout << "  Read-back (single-beat):\n";
    for (size_t i = 0; i < data.size(); i++) {
        uint32_t addr = 0x100 + i * 4;
        uint32_t val  = driver.read(addr);
        tick_and_sample(); tick_and_sample();
        std::cout << "    [0x" << std::hex << addr << "] = 0x" << val
                  << (val == data[i] ? " OK" : " MISMATCH") << std::dec << "\n";
    }
}

//============================================================
// Test 3: INCR burst read (4 beats)
//============================================================
static void test_incr_burst_read(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 3: INCR burst READ (4 beats)\n";
    std::cout << "────────────────────────────────────────\n";

    std::vector<uint32_t> expected = {0xCAFE0001, 0xCAFE0002, 0xCAFE0003, 0xCAFE0004};
    std::vector<uint32_t> got = driver.axi4_burst_read(0x100, 4, 1, 2, 1);
    tick_and_sample(); tick_and_sample();

    std::cout << "  Verify:\n";
    for (size_t i = 0; i < got.size(); i++) {
        bool ok = (i < expected.size() && got[i] == expected[i]);
        std::cout << "    Beat " << i << ": 0x" << std::hex << got[i]
                  << (ok ? " OK" : " MISMATCH") << std::dec << "\n";
    }
}

//============================================================
// Test 4: Write 8-beat INCR + burst read-back
//============================================================
static void test_long_burst_readback(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 4: 8-beat write + 8-beat burst read\n";
    std::cout << "────────────────────────────────────────\n";

    std::vector<uint32_t> data;
    for (int i = 0; i < 8; i++) data.push_back(0xAA000000 | i);
    driver.axi4_burst_write(0x180, data, 1, 2, 4);
    tick_and_sample(); tick_and_sample();

    std::vector<uint32_t> got = driver.axi4_burst_read(0x180, 8, 1, 2, 4);
    tick_and_sample(); tick_and_sample();

    std::cout << "  Verify:\n";
    for (size_t i = 0; i < got.size(); i++) {
        bool ok = (i < data.size() && got[i] == data[i]);
        std::cout << "    [0x" << std::hex << (0x180 + i*4) << "] = 0x"
                  << got[i] << (ok ? " OK" : " MISMATCH") << std::dec << "\n";
    }
}

//============================================================
// Test 5: FIXED burst write + FIXED burst read
//============================================================
static void test_fixed_burst_read(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 5: FIXED burst write + read\n";
    std::cout << "────────────────────────────────────────\n";

    std::vector<uint32_t> data = {0xDEAD0001, 0xDEAD0002, 0xDEAD0003};
    driver.axi4_burst_write(0x200, data, 0, 2, 2);
    tick_and_sample(); tick_and_sample();

    std::vector<uint32_t> got = driver.axi4_burst_read(0x200, 3, 0, 2, 2);
    tick_and_sample(); tick_and_sample();

    std::cout << "  FIXED read (all beats same addr 0x200):\n";
    for (size_t i = 0; i < got.size(); i++) {
        std::cout << "    Beat " << i << ": 0x" << std::hex << got[i]
                  << std::dec << (got[i] == 0xDEAD0003 ? " OK" : "") << "\n";
    }
}

//============================================================
// Test 6: WRAP burst write + read
//============================================================
static void test_wrap_burst_read(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 6: WRAP burst write + read\n";
    std::cout << "────────────────────────────────────────\n";

    std::vector<uint32_t> data = {0xBEEF0001, 0xBEEF0002, 0xBEEF0003, 0xBEEF0004};
    driver.axi4_burst_write(0x308, data, 2, 2, 3);
    tick_and_sample(); tick_and_sample();

    std::vector<uint32_t> got = driver.axi4_burst_read(0x308, 4, 2, 2, 3);
    tick_and_sample(); tick_and_sample();

    std::cout << "  WRAP read:\n";
    for (size_t i = 0; i < got.size(); i++) {
        bool ok = (i < data.size() && got[i] == data[i]);
        std::cout << "    Beat " << i << ": 0x" << std::hex << got[i]
                  << (ok ? " OK" : " MISMATCH") << std::dec << "\n";
    }
}

//============================================================
// Test 7: Single-beat burst read (ARLEN=0 edge case)
//============================================================
static void test_single_beat_burst_read(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 7: Single-beat burst read (ARLEN=0)\n";
    std::cout << "────────────────────────────────────────\n";

    driver.write(0x1FC, 0xFACEFACE, 7);
    tick_and_sample(); tick_and_sample();

    std::vector<uint32_t> got = driver.axi4_burst_read(0x1FC, 1, 1, 2, 7);
    tick_and_sample(); tick_and_sample();

    if (!got.empty())
        std::cout << "  [0x1FC] = 0x" << std::hex << got[0] << std::dec
                  << (got[0] == 0xFACEFACE ? " OK" : " MISMATCH") << "\n";
}

//============================================================
// Test 8: Multi-ID burst writes + reads
//============================================================
static void test_multi_id_reads(AxiLiteDriver& driver) {
    std::cout << "\n────────────────────────────────────────\n";
    std::cout << "TEST 8: Multi-ID burst writes + reads\n";
    std::cout << "────────────────────────────────────────\n";

    for (uint8_t id = 0; id < 4; id++) {
        uint32_t base = 0x100 + id * 0x20;
        std::vector<uint32_t> data = {
            (uint32_t)(0x10000000 * id + 0x01),
            (uint32_t)(0x10000000 * id + 0x02)
        };
        driver.axi4_burst_write(base, data, 1, 2, id);
        tick_and_sample(); tick_and_sample();

        std::vector<uint32_t> got = driver.axi4_burst_read(base, 2, 1, 2, id);
        tick_and_sample(); tick_and_sample();

        for (size_t i = 0; i < got.size(); i++) {
            bool ok = (i < data.size() && got[i] == data[i]);
            std::cout << "  ID " << (int)id << " beat " << i
                      << ": 0x" << std::hex << got[i]
                      << (ok ? " OK" : " MISMATCH") << std::dec << "\n";
        }
    }
}

//============================================================
// Master test sequence — CLEAN (no intentional violations)
//
// Tests 8 & 9 from the old sequence (WLAST early/missing
// error injection) are REMOVED because they create real
// bus-level protocol violations that WaveEye correctly flags.
// Those tests verified the SLAVE's error detection — not
// protocol compliance. For WaveEye clean runs, only
// compliant transactions are issued.
//============================================================
void run_fifo_sequence(AxiLiteDriver& driver) {
    test_single_beat_fifo(driver);          // FIFO path
    test_incr_write_and_readback(driver);   // Write burst + single reads
    test_incr_burst_read(driver);           // INCR burst read
    test_long_burst_readback(driver);       // 8-beat write + read
    test_fixed_burst_read(driver);          // FIXED burst read
    test_wrap_burst_read(driver);           // WRAP burst read
    test_single_beat_burst_read(driver);    // ARLEN=0 edge case
    test_multi_id_reads(driver);            // Multi-ID read verification
}
