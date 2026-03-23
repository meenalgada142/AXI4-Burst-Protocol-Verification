#============================================================
# Makefile for Verilator-based AXI4 Burst FIFO simulation
#============================================================
# WHAT CHANGED from AXI-Lite version:
#   1. rtl/axi_lite_fifo_wrapper.sv → rtl/axi4_burst_fifo_wrapper.sv
#   2. rtl/dut.v must be the NEW dut.v (with AWID/AWLEN/WLAST/BID ports)
#   3. Added 'run' and 'wave' convenience targets
#============================================================

TOP = dut

CXXFLAGS = -std=c++17 \
			-Iobj_dir \
			-I/mingw64/share/verilator/include \
			-I/mingw64/share/verilator/include/vltstd \
			-I. \
			-Isim \
			-I./include \
			-Wall \
			-Wno-sign-compare

SRC = sim/tb_top.cpp sim/driver.cpp sim/monitor.cpp sim/coverage_tracker.cpp sim/sequence.cpp
OBJ = $(SRC:.cpp=.o)

VERILATOR_SRC = \
	/mingw64/share/verilator/include/verilated.cpp \
	/mingw64/share/verilator/include/verilated_threads.cpp \
	/mingw64/share/verilator/include/verilated_vcd_c.cpp

#────────────────────────────────────────────────────────────
# RTL source list  *** THIS IS THE KEY CHANGE ***
#────────────────────────────────────────────────────────────
RTL_SRCS = rtl/dut_axi4.v rtl/axi4_burst_fifo_wrapper.sv rtl/fifo.sv

all: sim

#────────────────────────────────────────────────────────────
# Step 1: Verilate RTL → generates obj_dir/Vdut.h with
#         AWID, AWLEN, AWSIZE, AWBURST, WSTRB, WLAST, BID
#────────────────────────────────────────────────────────────
obj_dir/V$(TOP).cpp: $(RTL_SRCS)
	verilator -Wall -Wno-DECLFILENAME --top-module dut --trace --cc $(RTL_SRCS) \
		--exe $(SRC) \
		-CFLAGS "-I../include -std=c++17"
	make -C obj_dir -f V$(TOP).mk

#────────────────────────────────────────────────────────────
# Step 2: Compile C++ testbench sources
#────────────────────────────────────────────────────────────
sim/%.o: sim/%.cpp obj_dir/V$(TOP).cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

#────────────────────────────────────────────────────────────
# Step 3: Link simulation binary
#────────────────────────────────────────────────────────────
sim: $(OBJ) obj_dir/V$(TOP)__ALL.a
	$(CXX) $(CXXFLAGS) -o run_sim $(OBJ) $(VERILATOR_SRC) obj_dir/V$(TOP)__ALL.a

#────────────────────────────────────────────────────────────
# Run simulation
#────────────────────────────────────────────────────────────
run: sim
	@echo ""
	@echo "════════════════════════════════════════"
	@echo " Running AXI4 Burst Simulation"
	@echo "════════════════════════════════════════"
	./run_sim

#────────────────────────────────────────────────────────────
# Open waveform (install GTKWave: pacman -S mingw-w64-x86_64-gtkwave)
#────────────────────────────────────────────────────────────
wave: run
	gtkwave wave.vcd &

#────────────────────────────────────────────────────────────
# Clean
#────────────────────────────────────────────────────────────
clean:
	rm -rf obj_dir run_sim run_sim.exe wave.vcd sim/*.o *.o

.PHONY: all clean sim run wave