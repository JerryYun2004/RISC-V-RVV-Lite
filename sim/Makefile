# sim/Makefile  (Option B: override lint target)
.DEFAULT_GOAL := all

CVA5_DIR := $(abspath ../third_party/cva5)
CVA5_SIM_DIR := $(abspath ./obj_dir)

SIM_STATS_SV := $(CVA5_DIR)/test_benches/verilator/sim_stats.sv
CVA5_SIM_SV  := $(CVA5_DIR)/test_benches/verilator/cva5_sim.sv

include $(CVA5_DIR)/tools/cva5.mak

override CFLAGS += -DPC_TRACE_ON -DSTARTING_ADDR=0x80000000
override DETERMINISTIC := 1

# Add C++ preprocessor defines through CVa5's Verilator CFLAGS hook
override CFLAGS += -DPC_TRACE_ON
override DETERMINISTIC := 1


# Override the lint target from cva5.mak (it points to the wrong sim_stats path)
.PHONY: lint
lint:
	verilator -Wall -Wno-fatal -cc $(CVA5_HW_SRCS) \
		$(CVA5_SIM_SV) \
		$(SIM_STATS_SV) \
		--top-module cva5_sim \
		--lint-only

.PHONY: all
all: $(CVA5_SIM)

.PHONY: clean
clean:
	rm -rf $(CVA5_SIM_DIR)
