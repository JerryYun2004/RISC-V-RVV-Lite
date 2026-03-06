#include "Vcve2_top.h"
#include "verilated.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifndef DONE_MMIO_ADDR
#define DONE_MMIO_ADDR 0xFFFF0000u
#endif

#ifndef UART_MMIO_ADDR
#define UART_MMIO_ADDR 0x10000000u
#endif

static vluint64_t main_time = 0;
double sc_time_stamp() { return static_cast<double>(main_time); }

// -----------------------------
// Simple memory model
// -----------------------------
static constexpr uint32_t IMEM_BYTES = 1024 * 1024;  // 1 MiB
static constexpr uint32_t DMEM_BYTES = 1024 * 1024;  // 1 MiB
static std::vector<uint8_t> imem(IMEM_BYTES, 0);
static std::vector<uint8_t> dmem(DMEM_BYTES, 0);

static inline uint32_t load_le_u32(const std::vector<uint8_t>& mem, uint32_t addr) {
  if (addr + 3u >= mem.size()) return 0;
  return (uint32_t)mem[addr + 0u] |
         ((uint32_t)mem[addr + 1u] << 8) |
         ((uint32_t)mem[addr + 2u] << 16) |
         ((uint32_t)mem[addr + 3u] << 24);
}

static inline void store_le_u32(std::vector<uint8_t>& mem, uint32_t addr, uint32_t wdata, uint8_t be) {
  if (addr + 3u >= mem.size()) return;
  for (int i = 0; i < 4; i++) {
    if (be & (1u << i)) {
      mem[addr + (uint32_t)i] = (uint8_t)((wdata >> (8 * i)) & 0xFFu);
    }
  }
}

// -----------------------------
// Hex loader for "objcopy -O verilog"
// Supports:
//   @00000000
//   17 01 01 00 ...
// Bytes listed in increasing addresses.
// Returns number of bytes written.
// -----------------------------
static size_t load_objcopy_verilog_hex_bytes(const std::string& path, std::vector<uint8_t>& mem) {
  std::ifstream f(path);
  if (!f) return 0;

  std::string tok;
  uint32_t addr = 0;
  size_t written = 0;

  auto hex_to_u32 = [](const std::string& s) -> uint32_t {
    return (uint32_t)std::strtoul(s.c_str(), nullptr, 16);
  };

  while (f >> tok) {
    if (tok.empty()) continue;

    if (tok[0] == '@') {
      addr = hex_to_u32(tok.substr(1));
      continue;
    }

    uint32_t b = hex_to_u32(tok) & 0xFFu;
    if (addr < mem.size()) {
      mem[addr] = (uint8_t)b;
      written++;
    }
    addr++;
  }

  return written;
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0]
              << " <prog.hex> [--max-cycles N] [--print-every N] [--trace-if] [--trace-d]\n";
    return 1;
  }

  std::string hex_path = argv[1];
  uint64_t max_cycles = 200000;
  uint64_t print_every = 20000;
  bool trace_if = false;
  bool trace_d = false;

  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--max-cycles" && (i + 1) < argc) {
      max_cycles = std::stoull(argv[++i]);
    } else if (a == "--print-every" && (i + 1) < argc) {
      print_every = std::stoull(argv[++i]);
    } else if (a == "--trace-if") {
      trace_if = true;
    } else if (a == "--trace-d") {
      trace_d = true;
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      return 1;
    }
  }

  // Load program image
  imem.assign(IMEM_BYTES, 0);
  dmem.assign(DMEM_BYTES, 0);
  size_t loaded = load_objcopy_verilog_hex_bytes(hex_path, imem);
  // Safer bare-metal default: mirror image into DMEM too (overlapping region)
  std::memcpy(dmem.data(), imem.data(), std::min(imem.size(), dmem.size()));
  std::cout << "[TB] Loaded " << hex_path << " into IMEM (bytes=" << loaded << ")\n";

  Vcve2_top* dut = new Vcve2_top();

  // Static inputs
  dut->clk_i = 0;
  dut->rst_ni = 0;
  dut->fetch_enable_i = 0;
  dut->hart_id_i = 0;
  dut->boot_addr_i = 0x00000000;

  // Default bus inputs
  dut->instr_gnt_i = 0;
  dut->instr_rvalid_i = 0;
  dut->instr_rdata_i = 0;
  dut->instr_err_i = 0;

  dut->data_gnt_i = 0;
  dut->data_rvalid_i = 0;
  dut->data_rdata_i = 0;
  dut->data_err_i = 0;

  // Reset: a few cycles
  for (int i = 0; i < 10; i++) {
    dut->clk_i = 0; dut->eval(); main_time++;
    dut->clk_i = 1; dut->eval(); main_time++;
  }
  dut->rst_ni = 1;
  for (int i = 0; i < 5; i++) {
    dut->clk_i = 0; dut->eval(); main_time++;
    dut->clk_i = 1; dut->eval(); main_time++;
  }
  dut->fetch_enable_i = 1;
  std::cout << "[TB] Reset released, fetch_enable_i=1, starting simulation loop...\n";

  // ----------------------------
  // OBI 1-deep, fixed 1-cycle response per port (strict, Croc-style)
  // - Accept on req&&gnt (sampled at clk=0 eval)
  // - Respond next cycle with rvalid pulse (clk=0)
  // - Never assert gnt during rvalid
  // ----------------------------

  bool if_resp_due = false;
  uint32_t if_resp_addr = 0;

  bool d_resp_due = false;
  uint32_t d_resp_addr = 0;
  bool d_resp_is_write = false;

  // For debug, track last accepted
  uint64_t uart_chars = 0;
  bool done_seen = false;
  uint32_t done_value = 0;
  uint64_t done_cycle = 0;

  for (uint64_t cyc = 0; cyc < max_cycles; cyc++) {
    // ----------------------------
    // Phase A: clk low, drive subordinate inputs for this cycle, then eval
    // ----------------------------
    dut->clk_i = 0;

    // IF response
    dut->instr_rvalid_i = if_resp_due ? 1 : 0;
    dut->instr_err_i = 0;
    uint32_t if_insn = 0;
    if (if_resp_due) {
      if_insn = load_le_u32(imem, if_resp_addr);
      dut->instr_rdata_i = if_insn;
    } else {
      dut->instr_rdata_i = 0;
    }

    // D response
    dut->data_rvalid_i = d_resp_due ? 1 : 0;
    dut->data_err_i = 0;
    uint32_t d_rdata = 0;
    if (d_resp_due) {
      d_rdata = d_resp_is_write ? 0 : load_le_u32(dmem, d_resp_addr);
      dut->data_rdata_i = d_rdata;
    } else {
      dut->data_rdata_i = 0;
    }

    // Grants: only when not issuing rvalid on that port (strict 1-deep)
    dut->instr_gnt_i = (if_resp_due ? 0 : 1);
    dut->data_gnt_i  = (d_resp_due  ? 0 : 1);

    dut->eval();
    main_time++;

    // Optional traces (log what we *responded* this cycle)
    if (trace_if && if_resp_due) {
      std::cout << "[IF] resp pc=0x" << std::hex << std::setw(8) << std::setfill('0')
                << if_resp_addr << " insn=0x" << std::setw(8) << if_insn << std::dec << "\n";
    }
    if (trace_d && d_resp_due) {
      std::cout << "[D ] resp  addr=0x" << std::hex << std::setw(8) << std::setfill('0')
                << d_resp_addr << " (" << (d_resp_is_write ? "WR" : "RD") << ") rdata=0x"
                << std::setw(8) << d_rdata << std::dec << "\n";
    }

    // ----------------------------
    // Sample requests at clk low (after eval) and decide accepts
    // ----------------------------
    const bool if_fire = (dut->instr_req_o && dut->instr_gnt_i);
    const bool d_fire  = (dut->data_req_o  && dut->data_gnt_i);

    // Latch what we accepted now; schedule responses for next cycle.
    bool if_resp_due_next = false;
    uint32_t if_resp_addr_next = 0;

    bool d_resp_due_next = false;
    uint32_t d_resp_addr_next = 0;
    bool d_resp_is_write_next = false;

    if (if_fire) {
      if_resp_due_next = true;
      if_resp_addr_next = (uint32_t)dut->instr_addr_o;
      if (trace_if) {
        std::cout << "[IF] accept pc=0x" << std::hex << std::setw(8) << std::setfill('0')
                  << if_resp_addr_next << std::dec << "\n";
      }
    }

    if (d_fire) {
      const uint32_t addr = (uint32_t)dut->data_addr_o;
      const bool we = dut->data_we_o ? true : false;
      const uint32_t wdata = (uint32_t)dut->data_wdata_o;
      const uint8_t be = (uint8_t)dut->data_be_o;

      // Apply store side effects at accept time.
      if (we) {
        if (addr == UART_MMIO_ADDR) {
          char c = (char)(wdata & 0xFFu);
          std::cout << c << std::flush;
          uart_chars++;
        } else if (addr == DONE_MMIO_ADDR) {
          done_seen = true;
          done_value = wdata;
          done_cycle = cyc;
        } else {
          store_le_u32(dmem, addr, wdata, be);
        }
      }

      // Always provide a completion response next cycle (RD or WR).
      d_resp_due_next = true;
      d_resp_addr_next = addr;
      d_resp_is_write_next = we;

      if (trace_d) {
        std::cout << "[D ] accept addr=0x" << std::hex << std::setw(8) << std::setfill('0')
                  << addr << " (" << (we ? "WR" : "RD") << ") wdata=0x" << std::setw(8) << wdata
                  << " be=0x" << std::setw(2) << (uint32_t)be << std::dec << "\n";
      }
    }

    // Progress print (post-eval sampled at clk low)
    if (print_every != 0 && ((cyc % print_every) == 0)) {
      std::cout << "[TB] cyc=" << std::dec << cyc
                << " instr_req=" << (int)dut->instr_req_o
                << " instr_addr=0x" << std::hex << std::setw(8) << std::setfill('0') << (uint32_t)dut->instr_addr_o
                << " instr_gnt=" << std::dec << (int)dut->instr_gnt_i
                << " instr_rvalid=" << (int)dut->instr_rvalid_i
                << " data_req=" << (int)dut->data_req_o
                << " data_we=" << (int)dut->data_we_o
                << " data_addr=0x" << std::hex << std::setw(8) << std::setfill('0') << (uint32_t)dut->data_addr_o
                << " data_gnt=" << std::dec << (int)dut->data_gnt_i
                << " data_rvalid=" << (int)dut->data_rvalid_i
                << " sleep=" << (int)dut->core_sleep_o
                << " uart_chars=" << uart_chars
                << std::dec << "\n";
    }

    // ----------------------------
    // Phase B: rising edge
    // ----------------------------
    dut->clk_i = 1;
    dut->eval();
    main_time++;

    // Advance scheduled responses
    if_resp_due = if_resp_due_next;
    if_resp_addr = if_resp_addr_next;

    d_resp_due = d_resp_due_next;
    d_resp_addr = d_resp_addr_next;
    d_resp_is_write = d_resp_is_write_next;

    // Termination: after DONE is observed, run a few cycles to let core consume completion, then stop.
    if (done_seen && (cyc > (done_cycle + 4))) {
      std::cout << "[TB] DONE seen (addr=0x" << std::hex << DONE_MMIO_ADDR
                << ") value=0x" << std::setw(8) << std::setfill('0') << done_value
                << std::dec << " at cyc=" << done_cycle << "\n";
      break;
    }
  }

  std::cout << "[TB] Finished\n";
  dut->final();
  delete dut;
  return 0;
}
