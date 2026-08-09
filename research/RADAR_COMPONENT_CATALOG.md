# Open-Source Kintex-7 FPGA + Host PC FMCW/MIMO Radar: Component Catalog

Reference catalog for building an open-source FMCW/MIMO radar system around a Xilinx Kintex-7
FPGA (RFNoC 4.0 / Ettus USRP class hardware) and a Host PC. Consolidated from five prior research
documents, deduplicated, and reorganized as a lookup reference rather than a narrative report.

Target system baseline used throughout: Kintex-7 FPGA (as confirmed on a USRP B210), 2 Tx x 2 Rx
TDM-MIMO array via a single AD9361 transceiver (4-element virtual ULA), USB 3.0 host link, C++17 /
CUDA host stack.

---

## Table of Contents

1. [Overview & Architecture](#1-overview--architecture)
2. [Open-Source Repository Catalog](#2-open-source-repository-catalog)
3. [SDR Frameworks](#3-sdr-frameworks)
4. [RFNoC 4.0 Blocks](#4-rfnoc-40-blocks)
5. [FPGA IP Cores & Kintex-7 Resource Budget](#5-fpga-ip-cores--kintex-7-resource-budget)
6. [DSP Libraries](#6-dsp-libraries)
7. [FMCW/MIMO Radar-Specific Projects](#7-fmcwmimo-radar-specific-projects)
8. [MIMO / AoA / CFAR / Tracking Algorithms](#8-mimo--aoa--cfar--tracking-algorithms)
9. [FPGA-Host Interface Solutions](#9-fpga-host-interface-solutions)
10. [Host Streaming, IPC & Visualization](#10-host-streaming-ipc--visualization)
11. [Advanced / Experimental Techniques](#11-advanced--experimental-techniques)
12. [Patent-Derived Hardware & RF Improvements](#12-patent-derived-hardware--rf-improvements)
13. [FPGA Device Note](#13-fpga-device-note)
14. [Processing Allocation Matrix](#14-processing-allocation-matrix)
15. [System Performance Specifications](#15-system-performance-specifications)
16. [Implementation Roadmap](#16-implementation-roadmap)
17. [Cost Estimates](#17-cost-estimates)
18. [Risks & Mitigations](#18-risks--mitigations)
19. [Symbol Glossary](#19-symbol-glossary)
20. [References](#20-references)

---

## 1. Overview & Architecture

Processing multi-channel high-rate raw IQ streams (e.g. 2 receive channels x 100 MSps x 32-bit
complex IQ = 6.4 Gbps) is a streaming-bandwidth and compute problem before it is a radar-algorithm
problem. The workable architecture splits work by determinism and data rate:

- **FPGA (Kintex-7, RFNoC 4.0 fabric)**: everything that is high-rate, repetitive, and needs
  nanosecond-level determinism — FMCW de-chirping/digital downconversion (DDC), windowing, 1D range
  FFT, 2D Doppler FFT, corner-turn matrix transpose, TDM antenna switching, and (with surplus
  headroom) 2D CFAR detection.
- **Host PC**: everything that is dynamic, matrix-heavy, or benefits from floating point and large
  memory — super-resolution AoA (2D MUSIC, Capon/MVDR), point-cloud clustering (DBSCAN), Kalman/EKF
  tracking, and GPU visualization.

RFNoC 4.0 (Ettus Research's FPGA packet-routing framework, using VITA-49 CHDR packets and a CtrlPort
register bus) provides the FPGA-side infrastructure. On a Kintex-7 XC7K325T, this infrastructure
(crossbar, 10GbE, PCIe, radio core) consumes only about 20.6% of CLB logic and 4.8% of DSP48E1
slices, leaving over 800 DSP48E1 slices, 370 Block RAMs (36K), and 160,000 LUTs free for custom
radar IP.

A physical 2 Tx x 2 Rx antenna array, driven by Time-Division-Multiplexed (TDM) transmit switching,
synthesizes a 4-element virtual Uniform Linear Array (ULA), doubling angular resolution without
adding physical receivers. Inter-chirp target motion during TDM switching introduces a Doppler phase
error that must be corrected (Section 8) before spatial processing.

### Recommended end-to-end architecture

```
                    MASTER HETEROGENEOUS SYSTEM ARCHITECTURE
+---------------------------------------------------------------------------------------------------+
|  ANALOG RF FRONT-END & MIXER LAYER                                                                |
|  [TX1 / TX2 Antennas] <--- [FMCW Transceiver / AD9361-class RF] <--- [Rx1 / Rx2 Antennas]         |
|                                         | 200 MSps Raw ADC I/Q                                    |
+-----------------------------------------|---------------------------------------------------------+
                                          v
+---------------------------------------------------------------------------------------------------+
|  FPGA FIRMWARE LAYER (Xilinx Kintex-7 XC7K325T / Ettus RFNoC 4.0 @ 200 MHz)                       |
|                                                                                                    |
|  [TDM GPIO Sequencer] ---> [32-bit FMCW NCO Sweep] ---> [Mixer / De-Chirp]                        |
|        (5 ns State Machine)       (200 MHz LFM Ramp)               |                              |
|                                                                     v                              |
|  [VITA-49 CHDR Transport] <--- [Ping-Pong BRAM Matrix] <--- [5-Stage CIC Filter (R=8)]             |
|       (64-bit Packetizer)         (1024x128 Transpose)        (200 MSps -> 25 MSps)                |
+-------------------------------------------|--------------------------------------------------------+
                                            | VITA-49 CHDR over USB 3.0
                                            v
+---------------------------------------------------------------------------------------------------+
|  HOST PC ZERO-COPY STREAMING LAYER (C++17 / POSIX Real-Time Threads)                              |
|                                                                                                    |
|  [UHD USB 3.0 Driver] ---> [Cache-Padded SPSC Ring Buffer] ---> [Lock-Free Frame Dispatcher]        |
|   (SCHED_FIFO Core 2)         (Capacity 1024, alignas(64))          (Zero-Copy Handoff)             |
+----------------------------------------------------------------------------------------------------+
                                                                          |
                                                                          v
+---------------------------------------------------------------------------------------------------+
|  HOST DSP ENGINE & ADVANCED PROCESSING LAYER (C++17 / Eigen3 / CUDA / PyTorch TensorRT)           |
|                                                                                                    |
|  1. Doppler phase shift compensation: exp(-j 2*pi*tx*m_d / N_d)                                   |
|  2. 2D CA-CFAR & OS-CFAR (O(1) integral image, std::nth_element partial sort)                     |
|  3. 2D AoA suite: CORDIC phase monopulse & 2D MUSIC super-resolution                               |
|  4. Advanced techniques: cognitive RF subspace nulling, AI micro-Doppler CNN,                     |
|     compressed sensing OMP, OTA multi-static sync                                                 |
+---------------------------------------------------------------------------------------------------+
                                          |
                                          v Binary multi-part ZeroMQ (ipc:///tmp/radar_stream.ipc)
+---------------------------------------------------------------------------------------------------+
|  IPC STREAMING & VISUALIZATION LAYER                                                              |
|  [ZeroMQ Publisher] ---> Topics: radar/rd_matrix, radar/cfar_hits, radar/tracks                    |
|                             ---> OpenGL 3D point cloud & track GUI                                 |
+---------------------------------------------------------------------------------------------------+
```

### Core architectural principles

1. **Deterministic FPGA offloading**: high-rate repetitive datapaths (NCO sweep accumulation, CIC
   decimation, ping-pong corner-turn BRAM transpose, TDM antenna GPIO switching) run entirely in
   synthesizable Verilog/SystemVerilog inside the Kintex-7 fabric, consuming under 6.5% of DSPs and
   under 32% of Block RAM (see Section 5).
2. **Zero-copy ingestion over USB 3.0**: the host driver uses UHD's asynchronous USB bulk-transfer
   streaming path, landing frames in a 64-byte cache-line-padded SPSC ring buffer with atomic
   acquire/release semantics (Section 9).
3. **Aperture multiplication via TDM-MIMO**: orthogonal time-domain chirp interleaving between 2
   transmitters doubles the effective array aperture to 4 virtual half-wavelength-spaced elements;
   Doppler-induced inter-chirp phase drift is corrected with a precomputed phasor LUT (Section 8).
4. **Advanced techniques as optional extensions**: cognitive waveform interference nulling, AI
   micro-Doppler classification, compressed sensing, and multi-static synchronization sit on top of
   the classical pipeline without changing its core data path (Section 11).

---

## 2. Open-Source Repository Catalog

Fourteen open-source repositories spanning FPGA firmware RTL, host SDR drivers, GNU Radio
out-of-tree (OOT) modules, and specialized signal-processing libraries.

| # | Repository | Category | Language | License | Status | URL | Relevance |
|---|---|---|---|---|---|---|---|
| 1 | `ettusresearch/uhd` | FPGA firmware & host SDR | SystemVerilog, C++, Python | GPLv3 / Commercial | Active | github.com/EttusResearch/uhd | Core USRP driver, RFNoC 4.0 infrastructure, CHDR packet router, C++ `uhd::rx_streamer`, USB 3.0 streaming transport |
| 2 | `kit-cel/gr-radar` | Host SDR & DSP algorithms | C++, Python | GPLv3 | Maintenance (forks) | github.com/kit-cel/gr-radar | GNU Radio radar framework: FMCW/CW chirp generators, tagged-stream 1D/2D FFTs, peak finders, PyQtGraph sinks |
| 3 | `alexforencich/verilog-axis` / `verilog-dsp` | FPGA firmware | Verilog, SystemVerilog | MIT | Active | github.com/alexforencich/verilog-axis | Vendor-neutral AXI4-Stream RTL: streaming FIFOs, NCO/DDS generators, CIC decimation, FIR filters, complex multipliers |
| 4 | `analogdevicesinc/hdl` | FPGA firmware | Verilog, SystemVerilog | ADI BSD / GPL dual | Active | github.com/analogdevicesinc/hdl | Production SDR IP: digital downconverters, FIR decimation, AXI DMA controllers, Vivado IP Integrator TCL |
| 5 | `pulp-platform/fpga-fft` | FPGA firmware | SystemVerilog | Apache 2.0 / Solderpad | Active | github.com/pulp-platform/fpga-fft | Vendor-neutral streaming pipelined FFT and CORDIC math cores |
| 6 | `jgaeddert/liquid-dsp` | Host SDR & DSP algorithms | C (C99) | MIT | Active | github.com/jgaeddert/liquid-dsp | Zero-dependency C99 DSP: LFM chirp generators, Blackman-Harris/Kaiser windowing, polyphase filterbanks, FFT wrappers |
| 7 | `pysdr/pysdr` | Host SDR | Python (NumPy, SciPy) | MIT | Active | github.com/pysdr/pysdr | Educational/practical Python SDR reference: FMCW beat-frequency isolation, STFT, SAR backprojection |
| 8 | `anras/radar` | DSP algorithms | Python | MIT | Maintenance | github.com/anras/radar | FMCW & TDM/CDM MIMO simulation, 2D CA-CFAR/OS-CFAR detection, phase monopulse |
| 9 | `dineshlv/mmwave` | DSP algorithms | C, Python | BSD 3-Clause | Active | github.com/dineshlv/mmwave | Python/C port of TI mmWave SDK: 2D range-Doppler FFTs, 2D CA/OS-CFAR, phase monopulse, Capon/MUSIC wrappers, Doppler phase correction |
| 10 | `gnuradio/gr-specest` | Host SDR & DSP algorithms | C++, Armadillo | GPLv3 | Maintenance | github.com/gnuradio/gr-specest | High-resolution spectral estimation for MIMO arrays: MUSIC, Root-MUSIC, ESPRIT, Capon MVDR, spatial covariance |
| 11 | `petre-rodan/pyArgus` | DSP algorithms | Python (NumPy, SciPy) | MIT | Active | github.com/petre-rodan/pyArgus | Direction-finding: ULA/planar steering vector synthesis, Bartlett, Capon MVDR, 2D MUSIC, spatial smoothing |
| 12 | `radar-lab/pymwradar` | DSP algorithms | Python, C++ (pybind11) | Apache 2.0 | Active | github.com/radar-lab/pymwradar | 2x2/4x4 TDM-MIMO virtual array processing, Doppler phase correction, 2D OS-CFAR, DBSCAN clustering, EKF tracking |
| 13 | `radar-lab/openradar` | DSP algorithms | Python | MIT | Active | github.com/radar-lab/openradar | TI mmWave data processing: micro-Doppler extraction, 2D CA-CFAR, MUSIC AoA pseudospectrum, point-cloud generation |
| 14 | `pothosware/SoapySDR` | Host SDR | C++ | Boost 1.0 | Active | github.com/pothosware/SoapySDR | Vendor-neutral SDR hardware abstraction, portable multi-channel streaming across non-Ettus frontends |

---

## 3. SDR Frameworks

### 3.1 GNU Radio

| Category | Details |
|---|---|
| Website | gnuradio.org |
| License | GPLv3 |
| Language | C++, Python |
| Kintex-7 compatibility | Excellent, via RFNoC, gr-ettus, SoapySDR |
| Maturity | 20+ years, active |

Radar capabilities: FMCW waveform generation (signal source blocks), pulse compression (matched
filtering), MIMO processing (multi-channel via RFNoC), real-time FFT (VOLK-accelerated), CFAR
detection (via OOT modules), native USRP/RFNoC integration.

FPGA integration: RFNoC 4.0 support via UHD 4.0, gr-ettus blocks, SoapySDR plugin for multi-vendor
hardware, `gr-radar` OOT module for radar-specific processing, FPGA offload via RFNoC blocks.

Performance: DC to 60+ GHz sample rates (hardware dependent), up to 160 MHz bandwidth (USRP X410),
under 100 us FPGA-accelerated latency, up to 4 TX/4 RX channels on X410.

Recommended OOT modules:
- **gr-radar** (kit-cel/gr-radar): CFAR, pulse compression, target tracking blocks.
- **gr-digital** (bundled): digital modulation/demodulation for phase-coded waveforms.
- **gr-filter** (bundled): FIR/IIR filter design, VOLK-optimized.

Key blocks for radar processing chain: Signal Source -> DUC -> TX Radio -> (FPGA) -> RX Radio ->
DDC -> FFT -> Pulse Compression -> CFAR -> Tracking -> Visualization.

Version migration notes: GR 3.7 used deprecated SWIG bindings, Python 2.7, Boost.Thread; GR
3.8/3.9/3.10 moved to pybind11 native bindings, Python 3.10+, C++17, `std::shared_ptr`/`std::thread`,
CMake 3.16+.

VOLK (Vector-Optimized Library of Kernels) provides AVX2/AVX-512 kernels for radar-critical loops:
`volk_32fc_x2_multiply_32fc` (complex multiplication for matched filtering/de-chirping),
`volk_32fc_s32f_x2_power_32f` (power spectrum), `volk_32fc_32f_dot_product_32fc` (steering vector
dot products).

### 3.2 srsRAN

High-performance SDR suite, primarily cellular but adaptable.

| Category | Details |
|---|---|
| License | AGPLv3 (commercial licenses available) |
| Language | C++ |
| Kintex-7 compatibility | Limited — primarily cellular-focused, but adaptable |
| Maturity | 10+ years, professional support |

Capable of high-performance real-time SDR processing and multi-channel TX/RX paths, but lacks
radar-specific algorithms. Sample rates up to 120 MHz, bandwidth up to 100 MHz, latency under
500 us. Useful for prototyping, MIMO multi-channel handling, and performance benchmarking rather
than as a radar processing chain.

### 3.3 LimeSuite + SoapySDR

Open-source SDR ecosystem with FPGA gateware support.

| Category | Details |
|---|---|
| License | Apache 2.0, LGPL |
| Language | C++, Python |
| Kintex-7 compatibility | Good, via LMS7002M FPGA gateware (concepts transferable) |
| Maturity | 5+ years, active |

Capabilities: FMCW waveform generation, multi-channel synchronization, wideband processing up to
61.44 MHz, custom loadable FPGA gateware images. Sample rates up to 61.44 MSPS, 2 TX/2 RX channels
on LimeSDR.

Key components:
- **LimeSuite** (myriadrf/LimeSuite): device control, calibration, signal processing.
- **SoapySDR** (pothosware/SoapySDR): vendor-neutral unified API across SDR devices.
- **LimeSDR** hardware: Altera Cyclone IV FPGA (concepts transferable to Kintex-7), 100 kHz-3.8 GHz RF.

### 3.4 Pothos

Data-flow programming framework for SDR. Apache 2.0, C++/Python, moderate community. Modular
block-based architecture, multi-threaded, GPU-accelerated blocks available, SoapySDR integration.
Good for prototyping and as an alternative architecture to GNU Radio, but limited FPGA offload and
radar-specific features.

### 3.5 LuaRadio

Lightweight, embeddable SDR framework. MIT, Lua/C, small but active community. Small footprint,
real-time single-threaded/event-driven design, SoapySDR support. Suited to embedded platforms and
scripting-based configuration; limited FPGA support and radar feature set.

### 3.6 Red Pitaya

Open-source test-and-measurement platform. GPL, C/Python. Zynq-7010-based (ARM + FPGA), so concepts
are transferable to Kintex-7 rather than directly portable. Built-in arbitrary waveform generation,
oscilloscope, spectrum analyzer. 14-bit 125 MSPS dual-channel ADC/DAC, DC-50 MHz direct-sampling RF
range. Good for algorithm prototyping, test/measurement, and education.

### 3.7 Tier 3 / limited relevance

- **OpenBTS**: open-source cellular base station (AGPL, C++). Cellular-focused, minimal radar
  relevance.
- **BladeRF CLI**: command-line tools for BladeRF SDR (GPL, C). Basic TX/RX control only; BladeRF
  uses a Cyclone IV FPGA.
- **HackRF Tools**: open-source tools for HackRF (GPL, C). Basic SDR operations only; HackRF uses a
  MAX10 FPGA.

### 3.8 Comparison matrix

| Framework | License | Language | K7 support | FPGA accel | Radar features | Performance | Maturity |
|---|---|---|---|---|---|---|---|
| GNU Radio | GPLv3 | C++, Python | Excellent | Full (RFNoC 4.0) | Strong (gr-radar) | Good | Very high (20+ yrs) |
| srsRAN | AGPLv3 | C++ | Limited (cellular) | Partial | Weak | Excellent | Very high (10+ yrs) |
| LimeSuite + SoapySDR | Apache 2.0 | C++, Python | Good | Custom gateware | Moderate | Good | High (5+ yrs) |
| Pothos | Apache 2.0 | C++, Python | Moderate | Limited | Weak | Good | High (5+ yrs) |
| LuaRadio | MIT | Lua, C | Limited | Minimal | Weak | Fair | Moderate (3+ yrs) |
| Red Pitaya | GPL | C, Python | Limited (Zynq) | Moderate (transferable) | Moderate | Fair | High (5+ yrs) |
| OpenBTS | AGPL | C++ | Minimal | Minimal | Minimal | Fair | High (10+ yrs) |
| BladeRF CLI | GPL | C | Minimal | Minimal | Minimal | Fair | High (5+ yrs) |
| HackRF Tools | GPL | C | Minimal | Minimal | Minimal | Fair | High (5+ yrs) |

Primary pick: **GNU Radio + RFNoC 4.0** — native Kintex-7 support (all Gen-3 USRPs use Kintex-7
FPGAs), full RFNoC 4.0 FPGA acceleration, radar-specific OOT modules, large community, used in
production academic/commercial radar systems. **LimeSuite + SoapySDR** is the best secondary choice
for custom FPGA gateware work; **srsRAN** is worth considering only where its high-performance
multi-channel processing outweighs its cellular focus.

---

## 4. RFNoC 4.0 Blocks

RFNoC (RF Network-on-Chip) is Ettus Research's FPGA processing framework: AXI4-Stream packet
routing using a 64-bit CHDR header plus 32-bit IQ payload, and a 32-bit CtrlPort register bus for
software control. RFNoC 4.0 ships with UHD 4.0.

### 4.1 Core blocks

| Block | Function | Radar application | Notes |
|---|---|---|---|
| Radio | RF frontend interface (TX/RX) | TX/RX interface | Up to 160 MSPS, ~5-10K LUTs, ~20-40 DSPs, ~1-2 MB BRAM per radio, <100 ns latency |
| DUC/DDC | Digital up/down converters | FMCW chirp generation, IF processing, baseband conversion | NCO, CIC filters, half-band filters, resampling |
| FFT | Fast Fourier Transform acceleration | Range FFT, Doppler FFT, 2D FFT, pulse compression | Radix-2/4, streaming & burst modes, 64-65536 point sizes, 100+ MSPS, <10 us latency at 1024-pt |
| FIR | Finite Impulse Response filtering | Pulse shaping, anti-aliasing, matched filtering, windowing | 100+ MSPS |
| Replay | Record/playback via DRAM | Signal simulation, test waveform playback, data capture | DRAM-backed |
| Window | Hamming/Hann/Blackman windowing | Sidelobe suppression before FFT | |
| SigGen | Tone/chirp/noise/arbitrary waveform generation | Test signal injection | |
| KeepOneInN | Decimation by keeping 1-in-N | Rate reduction | |
| FIFO / DMA FIFO | AXI-Stream buffering, host-FPGA transfer | Data movement | |

### 4.2 Device compatibility

| Device series | FPGA family | RFNoC 4.0 support | Max channels | Max bandwidth |
|---|---|---|---|---|
| USRP X410 | Kintex-7 | Yes | 4 TX, 4 RX | 160 MHz |
| USRP X310 | Kintex-7 | Yes | 2 TX, 2 RX | 160 MHz |
| USRP X300 | Kintex-7 | Yes | 2 TX, 2 RX | 160 MHz |
| USRP N310 | Kintex-7 | Yes | 2 TX, 2 RX | 100 MHz |
| USRP N320 | Kintex-7 | Yes | 2 TX, 2 RX | 100 MHz |
| USRP E320 | Kintex-7 | Yes | 2 TX, 2 RX | 56 MHz |

Required tooling: UHD 4.0+ (github.com/EttusResearch/uhd), Vivado 2020.1+ for FPGA compilation,
GNU Radio 3.8+ for flowgraph development, RFNoC Modtool for custom OOT block creation.

### 4.3 RFNoC block specification example

A custom radar de-chirp block is declared in a `.block.yml` schema consumed by `rfnocmodtool`,
defining clock domains, CHDR data ports, and CtrlPort register offsets:

```yaml
# radar_dechirp.block.yml — Ettus RFNoC 4.0 block spec for FMCW de-chirp/decimation
schema: rfnoc_modtool_args
module_name: radar
version: "1.0"
rfnoc_version: "4.0"

blocks:
  radar_dechirp:
    clocks:
      - { name: rfnoc_chdr, freq: 200e6 }
      - { name: rfnoc_ctrl, freq: 100e6 }
    interface:
      data:
        inputs:  { in_adc:       { type: chdr, width: 64, format: sc16, port_num: 0 } }
        outputs: { out_dechirped: { type: chdr, width: 64, format: sc32, port_num: 0 } }
      control:
        ctrlport: { width: 32, byte_enable: false }
    registers:
      - { name: SWEEP_SLOPE,    addr: 0x0000, mode: rw, type: int, description: "32-bit FMCW LFM slope increment per 5 ns clock" }
      - { name: NCO_FREQ_START, addr: 0x0004, mode: rw, type: int, description: "32-bit NCO initial start-frequency phase increment" }
      - { name: DECIMATION_RATE,addr: 0x0008, mode: rw, type: int, description: "CIC decimation factor R (default 8)" }
      - { name: CHIRP_COUNT,    addr: 0x000C, mode: rw, type: int, description: "Slow-time chirps per CPI (default 128)" }
      - { name: TDM_CONFIG_1,   addr: 0x0010, mode: rw, type: int, description: "T_SETTLE[31:16] | T_SWEEP[15:0] in clock cycles" }
      - { name: TDM_CONFIG_2,   addr: 0x0014, mode: rw, type: int, description: "T_ADC_GATE[31:16] | T_PRT[15:0] in clock cycles" }
      - { name: TRIGGER_CTRL,   addr: 0x0018, mode: rw, type: int, description: "Bit0: SW trigger, Bit1: continuous run, Bit2: TDM reset" }
      - { name: STATUS,         addr: 0x001C, mode: ro, type: int, description: "Bit0: frame active, Bit1: buffer overflow" }
      - { name: DOPPLER_PHASE,  addr: 0x0020, mode: rw, type: int, description: "TDM Doppler phase alignment correction factor" }
```

---

## 5. FPGA IP Cores & Kintex-7 Resource Budget

### 5.1 Xilinx official IP cores (free with Vivado)

| IP core | Function | Notes |
|---|---|---|
| FFT (`xfft`, LogiCORE) | Fast Fourier Transform | Radix-2/4/8, streaming & burst I/O, all Kintex-7 devices |
| FIR Filter Compiler | Pulse compression, filtering | |
| DDS Compiler | Digital chirp/tone generation | 32-bit frequency/phase resolution, 200+ MHz output |
| DDR3 Memory Interface (`mig_7series`) | External memory controller | Up to 12.8 GB/s (64-bit @ 200 MHz), <10 clock cycle latency |
| AXI Interconnect / AXI DMA | On-chip data movement | |
| Clocking Wizard / Reset Generator | Clock/reset infrastructure | |

### 5.2 Open-source IP cores

- **CASPER** (casper.berkeley.edu, GPL/BSD): astronomy-proven DSP cores — FFT, FIR, DDS,
  packetizers, memory controllers, 10G Ethernet. 15+ years mature, Kintex-7 supported.
- **OpenXC7** (github.com/openXC7, MIT/BSD): Kintex-7-specific DDR3 controller, AXI infrastructure,
  DSP accelerators. Actively developed, 3+ years.
- **Radar-specific open cores**: pulse compression, CFAR detection, and beamforming IP exist as
  smaller community projects (search GitHub topics `fpga-radar`, `cfar-fpga`, `mimo-beamforming`) —
  verify maintenance status before depending on any single one.
- **LibreCores**: USB 3.0 and other general-purpose interface cores.
- **`alexforencich/verilog-axis`** and **`pulp-platform/fpga-fft`** (see Section 2): vendor-neutral
  AXI4-Stream building blocks and streaming FFT/CORDIC cores, MIT/Apache licensed.

### 5.3 IP resource utilization (XC7K325T)

| IP core | LUTs | DSPs | BRAM | Max clock | Throughput |
|---|---|---|---|---|---|
| FFT (1024-pt) | 3,500 | 20 | 256 KB | 200 MHz | 100+ MSPS |
| FIR (128 taps) | 2,000 | 8 | 16 KB | 200 MHz | 100+ MSPS |
| DDS (2 ch) | 1,500 | 4 | 8 KB | 250 MHz | 200+ MSPS |

### 5.4 Open-source FFT core comparison

Benchmarked as a 1024-point complex, 16-bit IQ pipelined-streaming FFT on Kintex-7 speed grade -2:

| FFT core | Source | Language | License | DSP48E1 | BRAM 36K | f_max | Vivado integration |
|---|---|---|---|---|---|---|---|
| Xilinx LogiCORE FFT | Xilinx/AMD | Encrypted VHDL | Proprietary | 12 | 6 | 350 MHz | Native IP Integrator |
| Ultra-FFT | open-src-builders | SystemVerilog | Apache 2.0 | 12 | 6 | 320 MHz | High (RTL import) |
| Spiral FFT Core | CMU Spiral team | Structural VHDL | BSD 3-Clause | 14 | 8 | 290 MHz | High (RTL import) |
| Vitis HLS FFT | Xilinx GitHub | C++ (HLS) | Apache 2.0 | 12 | 6 | 310 MHz | Native HLS IP core |
| Verilog-FFT | OpenCores | Verilog | LGPL | 16 | 10 | 240 MHz | Moderate (legacy RTL) |

Xilinx LogiCORE FFT v9.1 architecture options (XC7K325T-2, 1024-point complex 16-bit IQ):

| Architecture option | DSP48E1 | BRAM 36K | LUTs | FFs | f_max | Throughput/latency |
|---|---|---|---|---|---|---|
| Pipelined streaming I/O | 12 | 6 | 1,180 | 1,850 | 350 MHz | 1 sample/clock (continuous) |
| Radix-4 burst I/O | 4 | 3 | 820 | 1,120 | 315 MHz | Burst-phase execution |
| Radix-2 burst I/O | 3 | 3 | 690 | 940 | 300 MHz | Burst-phase execution |
| Radix-2 minimal resources | 1 | 2 | 540 | 780 | 275 MHz | Minimal footprint |

### 5.5 Detailed Kintex-7 XC7K325T radar-IP resource budget

XC7K325T total capacity: 203,800 CLB LUTs, 407,600 flip-flops, 840 DSP48E1 slices, 445 Block RAM
(36K) tiles (16.0 Mb on-chip RAM). For reference, XC7K410T offers 254,200 LUTs, 508,400 FFs, 1,540
DSPs, 795 BRAM tiles (28.6 Mb).

| Subsystem / IP core | CLB LUTs | FFs | DSP48E1 | BRAM (36K) | On-chip RAM | % LUTs | % DSPs | % BRAM |
|---|---|---|---|---|---|---|---|---|
| RFNoC 4.0 base infrastructure (crossbar, 10GbE MAC, PCIe, radio core) | 42,000 | 68,000 | 40 | 75 | 2.70 Mb | 20.61% | 4.76% | 16.85% |
| FMCW NCO sweep generator (2 ch) | 1,200 | 1,800 | 8 | 4 | 0.14 Mb | 0.59% | 0.95% | 0.90% |
| 5-stage CIC decimator (2 ch) | 2,400 | 3,200 | 8 | 4 | 0.14 Mb | 1.18% | 0.95% | 0.90% |
| Blackman-Harris windowing engine | 400 | 600 | 2 | 2 | 0.07 Mb | 0.20% | 0.24% | 0.45% |
| 1D range FFT (1024-pt pipelined) | 2,360 | 3,700 | 24 | 12 | 0.43 Mb | 1.16% | 2.86% | 2.70% |
| Corner-turning BRAM matrix transpose (dual 512 KB ping-pong) | 1,800 | 2,200 | 0 | 114 | 4.10 Mb | 0.88% | 0.00% | 25.62% |
| 2D Doppler FFT (128-pt pipelined) | 1,400 | 1,900 | 12 | 6 | 0.21 Mb | 0.69% | 1.43% | 1.35% |
| CORDIC beamforming & phase compensation | 1,850 | 2,400 | 0 | 0 | 0.00 Mb | 0.91% | 0.00% | 0.00% |
| TDM GPIO timing sequencer | 350 | 450 | 0 | 0 | 0.00 Mb | 0.17% | 0.00% | 0.00% |
| **Total radar IP subsystem** | **11,760** | **16,250** | **54** | **142** | **5.09 Mb** | **5.77%** | **6.43%** | **31.91%** |
| **Combined system total (RFNoC + radar)** | **53,760** | **84,250** | **94** | **217** | **7.79 Mb** | **26.38%** | **11.19%** | **48.76%** |
| **Surplus unallocated capacity** | **150,040** | **323,350** | **746** | **228** | **8.21 Mb** | **73.62%** | **88.81%** | **51.24%** |

Takeaways: the corner-turn matrix transpose dominates BRAM usage (25.6% alone, to hold a
1024x128 complex ping-pong frame in on-chip RAM with no external DDR3 access). The full radar
acceleration chain uses only 6.4% of DSP slices. The large surplus (74% LUTs, 89% DSPs, 51% BRAM)
comfortably fits onboard 2D CA-CFAR detection and spatial covariance-matrix preprocessing directly
in the FPGA, beyond what's budgeted above.

### 5.6 RTL excerpts

#### SystemVerilog global package (`radar_pkg.sv`)

Central parameter/register/state-machine definitions shared across all radar RTL modules:

```systemverilog
package radar_pkg;
  // Clock & timing
  parameter real    FABRIC_CLK_FREQ_HZ   = 200_000_000.0; // 200 MHz system clock
  parameter integer TIME_PRECISION_PS    = 5000;          // 5 ns edge precision

  // FMCW / MIMO waveform parameters
  parameter integer FAST_TIME_NFFT       = 1024; // 1D range FFT size
  parameter integer SLOW_TIME_NFFT       = 128;  // 2D Doppler FFT size (chirps/CPI)
  parameter integer NUM_TX_CHANNELS      = 2;
  parameter integer NUM_RX_CHANNELS      = 2;
  parameter integer NUM_VIRTUAL_CHANNELS = 4;    // 2x2 TDM virtual ULA elements

  parameter integer DEFAULT_T_SETTLE     = 200;   // 1.0 us TX settling
  parameter integer DEFAULT_T_SWEEP      = 10240; // 51.2 us ramp sweep
  parameter integer DEFAULT_T_ADC_GATE   = 10240; // 51.2 us sampling window
  parameter integer DEFAULT_T_PRT        = 12000; // 60.0 us pulse repetition time

  // Bit-width precision
  parameter integer ADC_DATA_WIDTH       = 16;
  parameter integer NCO_PHASE_WIDTH      = 32;
  parameter integer NCO_LUT_ADDR_WIDTH   = 14;
  parameter integer CIC_STAGES           = 5;
  parameter integer CIC_DECIM_R          = 8;
  parameter integer CIC_ACCUM_WIDTH      = 32;   // 16 + N*log2(R*M) = 16+15 -> 32-bit
  parameter integer CHDR_DATA_WIDTH      = 64;   // VITA-49 CHDR bus width
  parameter integer CTRL_DATA_WIDTH      = 32;   // RFNoC CtrlPort bus width
  parameter integer BRAM_TILES_TOTAL     = 114;  // dual ping-pong BRAM36K tile count

  // RFNoC CtrlPort register map
  typedef enum logic [31:0] {
    REG_SWEEP_SLOPE     = 32'h0000_0000,
    REG_NCO_START_FREQ  = 32'h0000_0004,
    REG_DECIM_RATE      = 32'h0000_0008,
    REG_CHIRP_COUNT     = 32'h0000_000C,
    REG_TDM_CONFIG_1    = 32'h0000_0010, // T_SETTLE[31:16] | T_SWEEP[15:0]
    REG_TDM_CONFIG_2    = 32'h0000_0014, // T_ADC_GATE[31:16] | T_PRT[15:0]
    REG_TRIGGER_CTRL    = 32'h0000_0018,
    REG_STATUS          = 32'h0000_001C,
    REG_DOPPLER_PHASE   = 32'h0000_0020
  } reg_addr_e;

  // TDM sequencer FSM states
  typedef enum logic [2:0] {
    ST_IDLE, ST_SET_TX, ST_SWEEP, ST_ADC_GATE, ST_INTER_CHIRP
  } tdm_state_e;

  // VITA-49 CHDR packet header (64-bit RFNoC 4.0 format)
  typedef struct packed {
    logic [3:0]  pkt_type;    // 0=Data, 1=Context
    logic [3:0]  num_pkts;
    logic        has_time;
    logic        eob;         // end of burst
    logic [5:0]  vc_id;
    logic [15:0] seq_num;
    logic [16:0] payload_len; // length in 64-bit words
    logic [14:0] stream_id;
  } chdr_header_t;

  typedef struct packed { logic signed [15:0] i; logic signed [15:0] q; } cplx16_t;
  typedef struct packed { logic signed [31:0] i; logic signed [31:0] q; } cplx32_t;
endpackage : radar_pkg
```

#### FMCW NCO sweep generator (`fmcw_nco_sweep.v`)

Generates a continuous LFM digital chirp at 200 MHz using a 32-bit phase accumulator (frequency
resolution `f_clk / 2^32` = 0.0465 Hz) and a 14-bit truncated sine/cosine LUT (SFDR >84 dBc, >100
dBc with noise shaping). Each Kintex-7 DSP48E1 slice implements a 25x18-bit multiply; a full 16-bit
complex multiplier uses 4 DSP48E1 slices and can run up to ~450 MHz.

```verilog
module fmcw_nco_sweep #(
    parameter integer PHASE_WIDTH = 32,
    parameter integer AMPL_WIDTH  = 16
)(
    input  wire                   clk, reset, sweep_trigger,
    input  wire [PHASE_WIDTH-1:0] start_freq_inc, slope_inc,
    input  wire [15:0]            sweep_len_clk,
    output wire                   sweep_active, sweep_done,
    output reg  signed [AMPL_WIDTH-1:0] nco_i, nco_q,
    output reg                    out_valid
);
  reg [PHASE_WIDTH-1:0] phase_accum, current_freq_inc;
  reg [15:0]            clk_cnt;
  reg                   active_reg, done_reg;
  reg signed [AMPL_WIDTH-1:0] lut_sin [0:(1<<12)-1];
  reg signed [AMPL_WIDTH-1:0] lut_cos [0:(1<<12)-1];
  // lut_sin/lut_cos initialized from sin/cos(2*pi*i/4096) * 32767

  assign sweep_active = active_reg;
  assign sweep_done   = done_reg;

  always @(posedge clk) begin
    if (reset) begin
      phase_accum <= 0; current_freq_inc <= 0; clk_cnt <= 0;
      active_reg <= 0; done_reg <= 0; out_valid <= 0;
    end else begin
      done_reg <= 1'b0;
      if (sweep_trigger && !active_reg) begin
        active_reg <= 1'b1; phase_accum <= 0;
        current_freq_inc <= start_freq_inc; clk_cnt <= 0; out_valid <= 1'b1;
      end else if (active_reg) begin
        if (clk_cnt >= sweep_len_clk - 1) begin
          active_reg <= 1'b0; done_reg <= 1'b1; out_valid <= 1'b0;
        end else begin
          clk_cnt <= clk_cnt + 1'b1;
          current_freq_inc <= current_freq_inc + slope_inc;
          phase_accum <= phase_accum + current_freq_inc;
          out_valid <= 1'b1;
        end
      end else out_valid <= 1'b0;

      if (out_valid) begin // phase-to-amplitude translation
        nco_i <= lut_cos[phase_accum[PHASE_WIDTH-1 -: 12]];
        nco_q <= lut_sin[phase_accum[PHASE_WIDTH-1 -: 12]];
      end
    end
  end
endmodule
```

#### Multiplierless 5-stage CIC decimator (`cic_decimator.v`)

Reduces 200 MSps ADC streams to 25 MSps (R=8) with no multipliers. Bit growth across 5
integrator/comb stages is `N*log2(R*M) = 5*log2(8) = 15` bits; 16-bit input samples grow to 31 bits,
which fits a 32-bit two's-complement accumulator without overflow (Hogenauer's theory).

```verilog
module cic_decimator #(
    parameter integer IN_WIDTH = 16, OUT_WIDTH = 32, STAGES = 5, DECIM_R = 8
)(
    input  wire clk, reset, in_valid,
    input  wire signed [IN_WIDTH-1:0] in_i, in_q,
    output reg  out_valid,
    output reg  signed [OUT_WIDTH-1:0] out_i, out_q
);
  reg signed [OUT_WIDTH-1:0] int_i [0:STAGES-1], int_q [0:STAGES-1];
  reg [2:0] rate_cnt;
  reg       strobe_25mhz;
  reg signed [OUT_WIDTH-1:0] comb_in_i, comb_in_q;
  reg signed [OUT_WIDTH-1:0] comb_i [0:STAGES-1], comb_q [0:STAGES-1];
  reg signed [OUT_WIDTH-1:0] comb_d_i [0:STAGES-1], comb_d_q [0:STAGES-1];
  integer k;

  // Integrator stage pipeline, runs at 200 MHz
  always @(posedge clk) begin
    if (reset) begin
      for (k = 0; k < STAGES; k = k + 1) begin int_i[k] <= 0; int_q[k] <= 0; end
      rate_cnt <= 0; strobe_25mhz <= 0;
    end else if (in_valid) begin
      int_i[0] <= int_i[0] + in_i; int_q[0] <= int_q[0] + in_q;
      for (k = 1; k < STAGES; k = k + 1) begin
        int_i[k] <= int_i[k] + int_i[k-1];
        int_q[k] <= int_q[k] + int_q[k-1];
      end
      if (rate_cnt == DECIM_R - 1) begin
        rate_cnt <= 0; strobe_25mhz <= 1'b1;
        comb_in_i <= int_i[STAGES-1]; comb_in_q <= int_q[STAGES-1];
      end else begin rate_cnt <= rate_cnt + 1'b1; strobe_25mhz <= 1'b0; end
    end else strobe_25mhz <= 1'b0;
  end

  // Comb stage pipeline, runs at the 25 MHz decimated strobe rate
  always @(posedge clk) begin
    if (reset) begin
      for (k = 0; k < STAGES; k = k + 1) begin
        comb_i[k] <= 0; comb_q[k] <= 0; comb_d_i[k] <= 0; comb_d_q[k] <= 0;
      end
      out_valid <= 0;
    end else if (strobe_25mhz) begin
      comb_i[0] <= comb_in_i - comb_d_i[0]; comb_q[0] <= comb_in_q - comb_d_q[0];
      comb_d_i[0] <= comb_in_i; comb_d_q[0] <= comb_in_q;
      for (k = 1; k < STAGES; k = k + 1) begin
        comb_i[k] <= comb_i[k-1] - comb_d_i[k]; comb_q[k] <= comb_q[k-1] - comb_d_q[k];
        comb_d_i[k] <= comb_i[k-1]; comb_d_q[k] <= comb_q[k-1];
      end
      out_valid <= 1'b1; out_i <= comb_i[STAGES-1]; out_q <= comb_q[STAGES-1];
    end else out_valid <= 1'b0;
  end
endmodule
```

#### Ping-pong corner-turn matrix transpose (`corner_turn_matrix.v`)

Transposes the fast-time range-FFT output (written row-by-row, one row per chirp) into slow-time
Doppler-FFT input (read column-by-column, one column per range bin). Two 512 KB Block RAM arrays
(`bram_a`, `bram_b`) each hold 1024x128 = 131,072 32-bit complex words (114 BRAM36K tiles total,
1.024 MB), executing entirely on-chip with no external DDR3 traffic.

```verilog
module corner_turn_matrix #(
    parameter integer N_RANGE = 1024, N_CHIRP = 128, DATA_W = 32
)(
    input  wire clk, reset,
    input  wire s_axis_tvalid, input wire [DATA_W-1:0] s_axis_tdata, input wire s_axis_tlast,
    output wire s_axis_tready,
    output reg  m_axis_tvalid, output reg [DATA_W-1:0] m_axis_tdata, output reg m_axis_tlast,
    input  wire m_axis_tready,
    output reg  frame_complete_pulse
);
  localparam TOTAL_SAMPLES = N_RANGE * N_CHIRP;
  (* ram_style = "block" *) reg [DATA_W-1:0] bram_a [0:TOTAL_SAMPLES-1];
  (* ram_style = "block" *) reg [DATA_W-1:0] bram_b [0:TOTAL_SAMPLES-1];

  reg [16:0] wr_addr; reg [9:0] wr_range_cnt; reg [6:0] wr_chirp_cnt;
  reg [9:0]  rd_range_cnt; reg [6:0] rd_chirp_cnt;
  wire [16:0] rd_addr = (rd_chirp_cnt * N_RANGE) + rd_range_cnt; // transposed address
  reg ping_pong_sel, buffer_ready_to_read, reading_active;

  assign s_axis_tready = 1'b1;

  // Write: fast-time row-by-row sequential write; swap ping-pong buffer at frame end
  always @(posedge clk) begin
    if (reset) begin
      wr_addr <= 0; wr_range_cnt <= 0; wr_chirp_cnt <= 0;
      ping_pong_sel <= 0; buffer_ready_to_read <= 0; frame_complete_pulse <= 0;
    end else begin
      frame_complete_pulse <= 1'b0;
      if (s_axis_tvalid && s_axis_tready) begin
        if (ping_pong_sel == 1'b0) bram_a[wr_addr] <= s_axis_tdata;
        else                        bram_b[wr_addr] <= s_axis_tdata;
        if (wr_range_cnt == N_RANGE - 1) begin
          wr_range_cnt <= 0;
          if (wr_chirp_cnt == N_CHIRP - 1) begin
            wr_chirp_cnt <= 0; wr_addr <= 0;
            ping_pong_sel <= ~ping_pong_sel;
            buffer_ready_to_read <= 1'b1; frame_complete_pulse <= 1'b1;
          end else begin wr_chirp_cnt <= wr_chirp_cnt + 1'b1; wr_addr <= wr_addr + 1'b1; end
        end else begin wr_range_cnt <= wr_range_cnt + 1'b1; wr_addr <= wr_addr + 1'b1; end
      end
    end
  end

  // Read: slow-time column-by-column transposed read from the buffer not being written
  always @(posedge clk) begin
    if (reset) begin
      rd_range_cnt <= 0; rd_chirp_cnt <= 0; m_axis_tvalid <= 0; m_axis_tlast <= 0; reading_active <= 0;
    end else begin
      if (buffer_ready_to_read && !reading_active) begin
        reading_active <= 1'b1; rd_range_cnt <= 0; rd_chirp_cnt <= 0;
      end
      if (reading_active && (m_axis_tready || !m_axis_tvalid)) begin
        m_axis_tvalid <= 1'b1;
        m_axis_tdata  <= (ping_pong_sel == 1'b0) ? bram_b[rd_addr] : bram_a[rd_addr];
        m_axis_tlast  <= (rd_chirp_cnt == N_CHIRP - 1);
        if (rd_chirp_cnt == N_CHIRP - 1) begin
          rd_chirp_cnt <= 0;
          if (rd_range_cnt == N_RANGE - 1) begin rd_range_cnt <= 0; reading_active <= 0; end
          else rd_range_cnt <= rd_range_cnt + 1'b1;
        end else rd_chirp_cnt <= rd_chirp_cnt + 1'b1;
      end else if (!reading_active) begin m_axis_tvalid <= 0; m_axis_tlast <= 0; end
    end
  end
endmodule
```

#### Deterministic TDM-MIMO GPIO timing controller (`tdm_gpio_sequencer.v`)

The core timing controller for 2x2 TDM-MIMO transmit switching. Host operating systems introduce
microsecond-scale USB/Ethernet jitter; this FPGA state machine runs synchronously with the 200 MHz
fabric clock and delivers 5 ns deterministic edge timing for TX1/TX2 antenna enables, the FMCW
sweep trigger, and the ADC sampling gate.

```verilog
module tdm_gpio_sequencer #(
    parameter integer CLK_FREQ_HZ = 200_000_000
)(
    input  wire clk, reset, start_frame,
    input  wire [15:0] t_settle, t_sweep, t_adc_gate, t_prt, // cycles; defaults 200/10240/10240/12000
    input  wire [6:0]  n_chirps,                              // default 128
    output reg  tx1_gpio_en, tx2_gpio_en, fmcw_trigger, adc_gate_en,
    output reg  [6:0] current_chirp,
    output reg  frame_active
);
  localparam ST_IDLE = 3'b000, ST_SET_TX = 3'b001, ST_SWEEP = 3'b010,
             ST_ADC_GATE = 3'b011, ST_INTER_CHIRP = 3'b100;
  reg [2:0]  state;
  reg [15:0] timer_cnt;
  reg        tx_select; // 0: TX1 active, 1: TX2 active

  always @(posedge clk) begin
    if (reset) begin
      state <= ST_IDLE; timer_cnt <= 0; tx_select <= 0;
      tx1_gpio_en <= 0; tx2_gpio_en <= 0; fmcw_trigger <= 0; adc_gate_en <= 0;
      current_chirp <= 0; frame_active <= 0;
    end else begin
      fmcw_trigger <= 1'b0;
      case (state)
        ST_IDLE: begin
          tx1_gpio_en <= 0; tx2_gpio_en <= 0; adc_gate_en <= 0;
          frame_active <= 0; current_chirp <= 0; timer_cnt <= 0;
          if (start_frame) begin frame_active <= 1'b1; tx_select <= 0; state <= ST_SET_TX; end
        end
        ST_SET_TX: begin
          tx1_gpio_en <= ~tx_select; tx2_gpio_en <= tx_select;
          if (timer_cnt >= t_settle - 1) begin
            timer_cnt <= 0; fmcw_trigger <= 1'b1; state <= ST_SWEEP;
          end else timer_cnt <= timer_cnt + 1'b1;
        end
        ST_SWEEP: begin // brief settle after trigger, then open the ADC gate
          if (timer_cnt >= 16'd10) begin adc_gate_en <= 1'b1; state <= ST_ADC_GATE; timer_cnt <= 0; end
          else timer_cnt <= timer_cnt + 1'b1;
        end
        ST_ADC_GATE: begin
          if (timer_cnt >= t_adc_gate - 1) begin
            adc_gate_en <= 0; timer_cnt <= 0; state <= ST_INTER_CHIRP;
          end else timer_cnt <= timer_cnt + 1'b1;
        end
        ST_INTER_CHIRP: begin
          if (timer_cnt >= (t_prt - t_settle - t_adc_gate) - 1) begin
            timer_cnt <= 0;
            if (current_chirp == n_chirps - 1) begin
              state <= ST_IDLE; tx1_gpio_en <= 0; tx2_gpio_en <= 0; frame_active <= 0;
            end else begin
              current_chirp <= current_chirp + 1'b1;
              tx_select <= ~tx_select; // alternate TDM channel: TX1 <-> TX2
              state <= ST_SET_TX;
            end
          end else timer_cnt <= timer_cnt + 1'b1;
        end
        default: state <= ST_IDLE;
      endcase
    end
  end
endmodule
```

CORDIC engines complement the above for beamforming and monopulse, performing vector rotations and
phase/magnitude extraction with shift-and-add operations and no DSP multipliers:
- **Digital beamsteering (rotation mode)**: applies spatial phase shifts
  `e^{j*theta_k} = cos(theta_k) + j*sin(theta_k)`, `theta_k = (2*pi/lambda) * d_k * sin(phi)`.
- **Monopulse phase/angle extraction (vectoring mode)**: given `I + jQ`, computes magnitude
  `R = sqrt(I^2+Q^2)` and phase `phi = arctan(Q/I)` in about 20 clock cycles.

### 5.7 Vivado build automation

Full-project TCL build script (`build_rfnoc_radar.tcl`) targets `xc7k325tffg900-2`, imports the RTL
sources above, generates a 200 MHz (5.000 ns) timing constraint, and runs synthesis/implementation:

```tcl
set project_name "rfnoc_radar_kintex7"
set device_part  "xc7k325tffg900-2"
create_project $project_name ./vivado_build -part $device_part -force
set_property target_language SystemVerilog [current_project]

add_files -norecurse [list \
    "rtl/radar_pkg.sv" "rtl/fmcw_nco_sweep.v" "rtl/cic_decimator.v" \
    "rtl/corner_turn_matrix.v" "rtl/tdm_gpio_sequencer.v" ]
update_compile_order -fileset sources_1

set fp [open "./vivado_build/timing_constraints.xdc" w]
puts $fp "create_clock -period 5.000 -name clk_200mhz \[get_ports clk\]"
puts $fp "set_input_delay -clock clk_200mhz 1.0 \[get_ports {in_i* in_q* s_axis_*}\]"
puts $fp "set_output_delay -clock clk_200mhz 1.0 \[get_ports {out_i* out_q* tx1_gpio_en tx2_gpio_en m_axis_*}\]"
close $fp
add_files -fileset constrs_1 -norecurse "./vivado_build/timing_constraints.xdc"

set_property -name {STEPS.SYNTH_DESIGN.ARGS.MORE OPTIONS} -value {-mode out_of_context} -objects [get_runs synth_1]
launch_runs synth_1 -jobs 8; wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 8; wait_on_run impl_1

open_run impl_1
report_utilization -file "./vivado_build/post_impl_utilization.rpt"
report_timing_summary -file "./vivado_build/post_impl_timing.rpt"
```

A companion out-of-context standalone script (`synth_kintex7.tcl`) synthesizes an individual module
(e.g. `corner_turn_matrix`) in isolation for early resource/timing validation, using
`synth_design -top <module> -mode out_of_context -flatten_hierarchy rebuilt`.

---

## 6. DSP Libraries

### 6.1 High-performance C++ libraries

| Library | Website/repo | License | Language | Notes |
|---|---|---|---|---|
| **KFR** | kfrlib.com, github.com/kfrlib/kfr | MIT, BSD-3-Clause | C++ (header-only, C API) | Fastest general-purpose FFT/filter library surveyed; 2-4x faster than FFTW for many sizes; FIR/IIR, resampling, window functions, convolution, filter design, tensor ops; SSE/AVX/AVX-512/NEON/RVV; benchmarked by LIGO/Virgo/KAGRA |
| **FFTW** | fftw.org | GPL | C | Industry-standard FFT performance; weaker general filter support |
| **Armadillo** | arma.sourceforge.net | Apache 2.0 | C++ | MATLAB-like linear algebra syntax |
| **Eigen** | eigen.tuxfamily.org | MPL2 | C++ | Template-based linear algebra, used for MIMO covariance/eigendecomposition (Section 8) |

### 6.2 Specialized DSP libraries

- **DSPFilters** (github.com/vinniefalco/DSPFilters, BSD-3-Clause): IIR filter design — Butterworth,
  Chebyshev, Elliptic, Optimum-L.
- **libdspl-2.0** (github.com/tschoonj/libdspl-2.0, LGPL): FFT, FIR/IIR, window functions, filter
  design, statistics, Hilbert transform.
- **VOLK** (github.com/gnuradio/volk, LGPL): SIMD-optimized DSP primitives (SSE/AVX/AVX-512/NEON),
  GNU Radio's acceleration layer — see Section 3.1.
- **liquid-dsp** (github.com/jgaeddert/liquid-dsp, MIT): zero-dependency C99 library —
  `chirp_create()`, `window_blackmanharris()`, polyphase filterbanks (`firpfb_crcf`). Well suited to
  embedded execution on edge platforms (NVIDIA Jetson, ARM Cortex-A78).

### 6.3 Python-based

- **NumPy** (numpy.org, BSD-3-Clause): good for prototyping; limited for hard real-time.
- **SciPy** (scipy.org, BSD-3-Clause): signal processing, FFT, filter design, window functions,
  spectral analysis.
- **PyFFTW**: FFTW bindings for Python.
- **MIT CoffeeCan / Lincoln Lab toolkits**: foundational heterodyne de-chirping, static-clutter MTI
  subtraction, SAR range-migration (omega-k) and backprojection imaging references.

### 6.4 Radar-specific DSP libraries

- **OpenRadar** (github.com/presenseradar/openradar, MIT, Python): complete FMCW processing chain,
  MIMO support (AoA estimation), noise removal, hardware abstraction, ML integration.
- **RadarLib** (github.com/radarlib/radarlib, BSD-3-Clause, C++ with Python bindings): pulse
  compression, CFAR detection, tracking, clutter suppression, SAR processing.
- **`anras/radar`** and **PyFMCW**: Python libraries for TDM/CDM MIMO simulation, 2D CFAR, and
  range-Doppler ambiguity modeling (see Section 2).

Rough performance ordering observed across the surveyed sources: FFT throughput KFR > FFTW >
libdspl-2.0 > VOLK > NumPy; filter throughput KFR > libdspl-2.0 > DSPFilters > VOLK > SciPy; ease of
use NumPy/SciPy > KFR > FFTW > libdspl-2.0 > VOLK.

**Recommended pairing**: KFR (or FFTW) in C++ for the host-side high-rate DSP path, Eigen3 for
matrix/eigendecomposition-heavy AoA algorithms, and OpenRadar/NumPy/SciPy for rapid Python
prototyping before porting to C++.

---

## 7. FMCW/MIMO Radar-Specific Projects

| Project | Repo/publication | License | Language | Features |
|---|---|---|---|---|
| OpenRadar (PreSense Radar) | github.com/presenseradar/openradar | MIT | Python | Complete FMCW chain, MIMO support, AoA estimation, range/Doppler processing, noise removal, ML integration |
| fmcw-RADAR (0xastro) | github.com/0xastro/fmcw-RADAR | MIT | C, Python | 1D/2D FFT, MIMO processing, DBSCAN clustering, extended Kalman filter |
| Radar Processing Library (RPL) | github.com/radar-processing/rpl | Apache 2.0 | C++, Python bindings | Pulse compression (LFM, phase-coded), CA/GO/SO-CFAR, tracking (Kalman, alpha-beta, IMM), MTI/STAP clutter suppression, SAR processing |
| RadarLib | github.com/radarlib/radarlib | BSD-3-Clause | C++, Python | See Section 6.4 |

Research/academic references worth consulting directly:
- "Design and implementation of a FPGA and DSP based MIMO radar imaging system" — 8 TX, 8 RX
  channels, real-time, Bayesian Matching Pursuit (ResearchGate 279450841).
- "Software-Defined Radio Beamforming System for 5G/Radar Applications" — ARM Cortex-A9 + Kintex-7
  FPGA, 64-channel massive MIMO digital beamforming at 27.5-28.35 GHz (ResearchGate 346245003).
- "Model Based Design of FMCW Radar Processing Systems on FPGA Platforms" — Xilinx Virtex-6 (concepts
  transferable to Kintex-7), multi-channel DDC, ADC interface, HLS-based design (ResearchGate 379108557).
- "High-Level synthesis assisted design and verification framework for automotive radar processors"
  — HLS-based automotive radar, Kintex-7 XC7K480T (ScienceDirect S0141933120304191).

For a fixed 2 TX / 2 RX, USB 3.0-connected board such as the B210, the practical component pick is
GNU Radio + RFNoC 4.0 for the framework, the Radio/DDC/FFT/FIR/Replay RFNoC blocks, KFR (or KFR +
FFTW) for host DSP, and OpenRadar/RadarLib for the radar-specific processing chain.

---

## 8. MIMO / AoA / CFAR / Tracking Algorithms

### 8.1 2x2 TDM-MIMO virtual array & Doppler phase compensation

Alternating transmission between Tx0 and Tx1 on sequential chirps (period `T_PRT`) synthesizes 4
virtual receive channels:

```
Physical Tx array:  [Tx0] <---------- d_tx = lambda ----------> [Tx1]
Physical Rx array:  [Rx0] <-- d_rx = lambda/2 --> [Rx1]

Virtual array:  [V0] <-- lambda/2 --> [V1] <-- lambda/2 --> [V2] <-- lambda/2 --> [V3]
```

Inter-chirp target motion at radial velocity `v_r` introduces a Doppler phase error on the channels
transmitted by Tx1 (V2, V3):

```
delta_phi_doppler = (4*pi*v_r*f0 / c) * T_PRT = 2*pi*f_d*T_PRT
```

Given the Doppler bin index `m_d`, the correction factor `Omega(m_d) = exp(-j*2*pi*m_d/N_c)` is
applied to V2 and V3 to restore phase coherence before spatial processing:

```
V2'(k_r, m_d) = V2(k_r, m_d) * Omega(m_d)
V3'(k_r, m_d) = V3(k_r, m_d) * Omega(m_d)
```

Steering vector formulations (half-wavelength virtual element spacing):

- **1D ULA azimuth**: `a(theta) = [1, e^{j*pi*sin(theta)}, e^{j*2*pi*sin(theta)}, e^{j*3*pi*sin(theta)}]^T`
- **Planar 2x2 (azimuth theta, elevation phi)**:
  `a(theta,phi) = [1, e^{j*pi*sin(theta)cos(phi)}, e^{j*pi*sin(phi)}, e^{j*pi*(sin(theta)cos(phi)+sin(phi))}]^T`
- **Inter-channel calibration**: `x_calibrated = C_cal^{-1} x_raw`, where
  `C_cal = diag(g0*e^{j*psi0}, g1*e^{j*psi1}, g2*e^{j*psi2}, g3*e^{j*psi3})` corrects per-channel gain
  imbalance `g` and phase error `psi`.

C++ implementation (`doppler_phase_correction.hpp/cpp`) — precomputes a per-(Doppler-bin, Tx)
correction LUT and applies it across the full range-Doppler-Tx-Rx tensor:

```cpp
class DopplerPhaseCorrector {
public:
    explicit DopplerPhaseCorrector(size_t num_doppler_bins, size_t num_tx = 2);
    void correct_phase_tensor(std::complex<float>* rd_tensor,
                               size_t num_range_bins, size_t num_rx = 2) const;
    std::complex<float> get_correction_factor(size_t doppler_bin_idx, size_t tx_idx) const;
private:
    size_t N_doppler_, N_tx_;
    std::vector<std::vector<std::complex<float>>> correction_lut_; // [tx][doppler_bin]

    void precompute_lut() {
        constexpr double PI = 3.14159265358979323846;
        for (size_t tx = 0; tx < N_tx_; ++tx)
            for (size_t m = 0; m < N_doppler_; ++m) {
                int m_shifted = static_cast<int>(m) - static_cast<int>(N_doppler_ / 2);
                double angle = -2.0 * PI * tx * m_shifted / static_cast<double>(N_doppler_);
                correction_lut_[tx][m] = { (float)std::cos(angle), (float)std::sin(angle) };
            }
    }
};

// Applies correction_lut_[tx][m] across every (range, doppler, tx, rx) tensor element,
// indexed as ((r * N_doppler_ + m) * N_tx_ + tx) * num_rx + rx.
```

Vectorized Python equivalent:

```python
import numpy as np

def correct_tdm_doppler_phase(rd_matrix: np.ndarray, n_tx: int = 2) -> np.ndarray:
    """rd_matrix shape: (N_range, N_doppler, N_tx, N_rx)."""
    n_range, n_doppler, _, n_rx = rd_matrix.shape
    m_doppler = np.arange(-n_doppler // 2, n_doppler // 2)
    tx_indices = np.arange(n_tx)
    phase_angles = -2.0 * np.pi * np.outer(m_doppler, tx_indices) / n_doppler
    correction_factors = np.exp(1j * phase_angles)          # (N_doppler, N_tx)
    return rd_matrix * correction_factors[np.newaxis, :, :, np.newaxis]
```

### 8.2 Angle-of-Arrival (AoA) algorithms

Four channel-count-4 (2x2 virtual array) AoA options, in increasing order of resolution and cost:

**1. Phase Monopulse** — `Delta_Phi = arg(x1 * x0*)`, `theta_hat = arcsin(Delta_Phi/pi)`.
O(N) per hit, CORDIC-compatible, but resolves only a single target per range-Doppler bin and is
noise-sensitive.

**2. Bartlett beamformer (delay-and-sum)** —
`P_Bartlett(theta) = a^H(theta) * R_xx * a(theta)`, with covariance
`R_xx = (1/L) * sum_l x_l x_l^H`. Robust at low SNR and works with a single snapshot (L=1), but has
a coarse Rayleigh resolution limit `~0.886*lambda/(N*d_v) ≈ 25.4 deg` for a 4-element array.

**3. Capon / MVDR adaptive beamformer** —
`P_Capon(theta) = 1 / (a^H(theta) * R_xx^{-1} * a(theta))`, regularized via diagonal loading
`R_dl = R_xx + gamma*I`. Higher resolution than Bartlett with adaptive null-forming, at the cost of
an O(N^3) matrix inversion.

**4. 2D MUSIC (super-resolution)** — eigendecompose
`R_xx = U_s*Lambda_s*U_s^H + U_n*Lambda_n*U_n^H` to get the noise subspace `U_n`, then
`P_MUSIC(theta,phi) = 1 / (a^H(theta,phi) * U_n * U_n^H * a(theta,phi))`. Resolves targets spaced
under 8 degrees apart, at the highest computational cost (eigendecomposition plus 2D grid search).

Trade-off summary:

| Metric | Phase Monopulse | Bartlett | Capon/MVDR | 2D MUSIC |
|---|---|---|---|---|
| Angular resolution | Low (1 target/bin) | Coarse (~25.4 deg) | Medium (~15 deg) | Super-resolution (<8 deg) |
| Complexity | O(N) per hit | O(M_theta * N) | O(N^3 + M_theta * N^2) | O(N^3 + M_theta*M_phi*N(N-K)) |
| Multi-target in same bin | No | Poor | Fair | Excellent |
| Min snapshots (L) | 1 | 1 | N (4+) | K+1 (4+) |
| Phase-error sensitivity | High | Low/robust | Moderate | High |
| Best execution target | FPGA RTL/CORDIC | FPGA HLS/DSP48 | Host CPU/GPU | Host CPU/GPU |

Phase Monopulse CORDIC implementation (`phase_monopulse_2d.hpp/cpp`) — computes azimuth/elevation
from a 4-element L-shaped virtual array `v0=(0,0)`, `v1=(lambda/2,0)` (azimuth pair),
`v2=(0,lambda/2)` (elevation pair), `v3=(lambda/2,lambda/2)`, using a 16-iteration CORDIC
`atan2` (no transcendental function calls):

```cpp
class PhaseMonopulse2D {
public:
    struct Result { float azimuth_deg, elevation_deg; bool valid; };

    Result compute_aoa(const std::array<std::complex<float>, 4>& v) const {
        auto az_cross = v[1] * std::conj(v[0]);
        float delta_phi_az = cordic_atan2(az_cross.imag(), az_cross.real());
        auto el_cross = v[2] * std::conj(v[0]);
        float delta_phi_el = cordic_atan2(el_cross.imag(), el_cross.real());

        float sin_az = delta_phi_az / M_PI, sin_el = delta_phi_el / M_PI;
        Result r{};
        if (std::abs(sin_az) <= 1.0f && std::abs(sin_el) <= 1.0f) {
            r.azimuth_deg = std::asin(sin_az) * (180.0f / M_PI);
            r.elevation_deg = std::asin(sin_el) * (180.0f / M_PI);
            r.valid = true;
        }
        return r;
    }

    // 16-iteration CORDIC vectoring-mode atan2(Q, I): shift-and-add rotation using a
    // precomputed atan(2^-k) LUT, quadrant-corrected at the end.
    float cordic_atan2(float Q, float I) const;
};
```

2D MUSIC super-resolution implementation (`music_2d_aoa.hpp/cpp`, Eigen3-based) — builds a
steering-vector LUT over an azimuth/elevation grid, forms the 4x4 spatial covariance matrix from L
snapshots, extracts the noise subspace via `Eigen::SelfAdjointEigenSolver`, and evaluates the MUSIC
pseudospectrum at each grid point:

```cpp
class Music2DAoA {
public:
    void compute_pseudospectrum(const Eigen::MatrixXcf& snapshots, size_t num_signals,
                                 std::vector<float>& pseudospectrum_out,
                                 size_t& num_az_bins, size_t& num_el_bins) const {
        float L = static_cast<float>(snapshots.cols());
        Eigen::Matrix4cf Rxx = (snapshots * snapshots.adjoint()) / L;
        Rxx += 1e-5f * Eigen::Matrix4cf::Identity(); // diagonal loading

        Eigen::SelfAdjointEigenSolver<Eigen::Matrix4cf> es(Rxx);
        auto eigenvectors = es.eigenvectors();
        size_t num_noise = 4 - std::min<size_t>(num_signals, 3);
        Eigen::MatrixXcf Un(4, num_noise);
        for (size_t i = 0; i < num_noise; ++i) Un.col(i) = eigenvectors.col(i);
        Eigen::Matrix4cf UnUnH = Un * Un.adjoint();

        for (size_t idx = 0; idx < steering_vectors_lut_.size(); ++idx) {
            auto denom = steering_vectors_lut_[idx].dot(UnUnH * steering_vectors_lut_[idx]);
            pseudospectrum_out[idx] = 10.0f * std::log10(1.0f / (std::abs(denom.real()) + 1e-12f));
        }
    }
    // find_peaks(): 8-neighbour local-maxima search over the (az, el) pseudospectrum grid,
    // sorted descending by power and truncated to max_peaks.
private:
    std::vector<Eigen::Vector4cf> steering_vectors_lut_; // a(0)=1, a(1)=e^{j*pi*u}, a(2)=e^{j*pi*v}, a(3)=e^{j*pi*(u+v)}
                                                          // u = sin(az)cos(el), v = sin(el)
};
```

### 8.3 2D target detection (CFAR)

```
                    2D SLIDING-WINDOW CFAR TOPOLOGY
+-----------------------------------------------------------------+
| Training cells (Nr)                                             |
|   +-----------------------------------------------------------+ |
|   | Guard cells (Ng)                                          | |
|   |   +-------------------------------------------------+     | |
|   |   | Cell Under Test (CUT)                            |     | |
|   |   +-------------------------------------------------+     | |
|   +-----------------------------------------------------------+ |
+-----------------------------------------------------------------+
```

**CA-CFAR (cell-averaging)**: noise estimate `P_n_hat = (1/N_ref) * sum(Z_m)` over reference cells;
threshold `T = alpha_CA * P_n_hat`, `alpha_CA = N_ref * (Pfa^{-1/N_ref} - 1)`. Optimal sensitivity in
homogeneous Rayleigh noise, but suffers target masking in multi-target scenes and false alarms at
clutter edges.

An O(1)-per-cell variant precomputes a 2D **integral image (prefix-sum matrix)**
`I(r,m) = P(r,m) + I(r-1,m) + I(r,m-1) - I(r-1,m-1)` in O(N_range*N_doppler) time, after which any
rectangular window sum is 4 lookups: `Sum = I(r2+1,m2+1) - I(r1,m2+1) - I(r2+1,m1) + I(r1,m1)`. Noise
power is `(Sum_outer - Sum_inner) / N_ref`.

**OS-CFAR (ordered-statistic)**: sorts reference cells `P_(1) <= ... <= P_(N_ref)`, picks rank
`k = floor(0.75 * N_ref)`, threshold `T = alpha_OS * P_(k)`. Immune to up to `N_ref - k` interfering
targets, at the cost of an O(N_ref log N_ref) sort per cell (or `std::nth_element` partial sort).

**GO-CFAR / SO-CFAR**: split the reference window into leading/lagging halves.
GO-CFAR (greatest-of) uses `max(mean_lead, mean_lag)` — suppresses false alarms at clutter
boundaries. SO-CFAR (smallest-of) uses `min(mean_lead, mean_lag)` — better resolves closely spaced
targets, at the cost of more false alarms.

| Variant | Pfa regulation | Multi-target resilience | Clutter-edge performance | Cost/cell | Best fit |
|---|---|---|---|---|---|
| CA-CFAR | Optimal (homogeneous) | Poor (masking) | Poor (false alarms) | Very low, O(N_ref) sum | FPGA HLS / BRAM line buffer |
| OS-CFAR | Moderate | Excellent | Good | High, O(N_ref log N_ref) sort | Host CPU/GPU or bitonic sort network |
| GO-CFAR | Good | Very poor | Excellent | Low, O(N_ref) + 1 compare | FPGA HLS pipeline |
| SO-CFAR | Poor | Good | Poor | Low, O(N_ref) + 1 compare | FPGA HLS pipeline |

C++ implementation (`cfar_2d.hpp/cpp`) — CA-CFAR via integral image, OS-CFAR via `std::nth_element`:

```cpp
struct Cfar2DParams {
    size_t guard_range = 2, guard_doppler = 2, train_range = 4, train_doppler = 4;
    double pfa = 1e-4, os_rank_percentile = 0.75, os_alpha_scale = 3.5;
};
struct DetectionHit { size_t range_bin, doppler_bin; float power_db, snr_db; };

class Cfar2DEngine {
public:
    explicit Cfar2DEngine(const Cfar2DParams& p) {
        size_t outer = (2*p.train_range+2*p.guard_range+1) * (2*p.train_doppler+2*p.guard_doppler+1);
        size_t inner = (2*p.guard_range+1) * (2*p.guard_doppler+1);
        N_ref_ = outer - inner;
        alpha_ca_ = N_ref_ * (std::pow(p.pfa, -1.0/N_ref_) - 1.0);
    }

    // detect_ca_cfar(): builds a double-precision integral image once, then for each CUT
    // computes outer-window and inner-guard-window sums via 4 lookups each, derives
    // noise_power = (outer_sum - inner_sum) / N_ref_, and flags cut_power > alpha_ca_ * noise_power.

    // detect_os_cfar(): for each CUT, gathers the N_ref_ reference-cell powers (excluding the
    // guard region), partial-sorts with std::nth_element to the target rank k_rank, and flags
    // cut_power > os_alpha_scale * ref_cells[k_rank].
private:
    Cfar2DParams params_; size_t N_ref_; double alpha_ca_;
};
```

Vectorized Python CA-CFAR using `scipy.ndimage.uniform_filter` as an efficient windowed-sum
operator:

```python
import numpy as np
from scipy.ndimage import uniform_filter

def ca_cfar_2d(power_matrix, guard_len=(2, 2), train_len=(4, 4), pfa=1e-4):
    gr, gd = guard_len; tr, td = train_len
    outer_shape = (2*(tr+gr)+1, 2*(td+gd)+1)
    inner_shape = (2*gr+1, 2*gd+1)
    n_ref = outer_shape[0]*outer_shape[1] - inner_shape[0]*inner_shape[1]

    sum_outer = uniform_filter(power_matrix.astype(np.float64), size=outer_shape, mode='constant') * np.prod(outer_shape)
    sum_inner = uniform_filter(power_matrix.astype(np.float64), size=inner_shape, mode='constant') * np.prod(inner_shape)
    noise_power = (sum_outer - sum_inner) / n_ref
    alpha = n_ref * (pfa**(-1.0/n_ref) - 1.0)
    threshold = alpha * noise_power

    mask = power_matrix > threshold
    edge_r, edge_d = tr+gr, td+gd
    valid = np.zeros_like(mask, dtype=bool)
    valid[edge_r:-edge_r, edge_d:-edge_d] = True
    return mask & valid
```

### 8.4 Target tracking

CFAR hits are first merged into target centroids `z_k = [r, theta, v_r]^T` via centroid clustering
(DBSCAN / center-of-mass), then associated to existing tracks by Gated Nearest Neighbor using
Mahalanobis distance `d_M^2 = (z_k - z_hat)^T * S^{-1} * (z_k - z_hat) <= gamma`.

**Alpha-beta filter** — steady-state linear tracker:
`predict: x_p = x_s + v_s*T_s, v_p = v_s`;
`update: x_s = x_p + alpha*(z - x_p), v_s = v_p + (beta/T_s)*(z - x_p)`.
Benedict-Bordner optimal tuning: `beta = 2*(2-alpha) - 4*sqrt(1-alpha)`.

**Extended Kalman Filter (EKF)** — tracks `x = [x, y, vx, vy]^T` in Cartesian coordinates while
linearizing the polar measurement model
`h(x) = [sqrt(x^2+y^2), atan2(y,x), (x*vx+y*vy)/sqrt(x^2+y^2)]^T` via the Jacobian:

```
H_k = [ x/r,           y/r,           0,   0
       -y/r^2,         x/r^2,         0,   0
        y(vx*y-vy*x)/r^3, x(vy*x-vx*y)/r^3, x/r, y/r ]
```

EKF loop: (1) state predict `x_hat = F * x_hat_prev`; (2) covariance predict
`P = F*P_prev*F^T + Q`; (3) innovation covariance `S = H*P*H^T + R`; (4) Kalman gain
`K = P*H^T*S^{-1}`; (5) state update `x_hat += K*(z - h(x_hat))`; (6) covariance update
`P = (I - K*H)*P`.

| Metric | Alpha-Beta | Linear Kalman | EKF | UKF |
|---|---|---|---|---|
| State space | Polar/Cartesian | Cartesian | Cartesian | Cartesian |
| Non-linear measurement handling | Linear conversion | Linear conversion | Direct linearization (Jacobian) | Unscented transform |
| Matrix computation | Scalar arithmetic | 4x4 matrix ops | 4x4 matrix mul + 3x3 inverse | Sigma points + 3x3 inverse |
| Maneuver accuracy | Low | Medium | High | Very high |
| Best execution target | FPGA RTL/MCU | Host CPU/ARM | Host CPU/ARM | Host CPU/GPU |

---

## 9. FPGA-Host Interface Solutions

### 9.1 USB 3.0 host interface

The B210 is a USB 3.0-only device — no PCIe or Ethernet host interface is present, so the host link
is fixed rather than a design choice.

| Solution | Type | Max throughput | Latency | Notes |
|---|---|---|---|---|
| Opal Kelly XEM7360 | USB 3.0 | 400+ MB/s | <100 us | Development-board form factor; representative of the class of USB 3.0 FPGA-host bridge the B210 uses |

USB 3.0's ~400+ MB/s sustained throughput comfortably covers the B210's actual streaming needs: at
its native 2x2 channel count and AD9361 sample rates (up to ~56/61.44 MSps), raw or lightly-decimated
IQ traffic sits well under this ceiling, and FPGA-side decimation/FFT offload (Section 4-5) reduces it
further before it ever reaches the host link.

### 9.2 UHD C++ streaming setup

Synchronous multi-channel streaming configuration via the Ettus USRP Hardware Driver (UHD) C++ API,
including external clock/time alignment and timed command scheduling:

```cpp
// Synchronous multi-channel streaming setup
uhd::stream_args_t stream_args("fc32", "sc16");
stream_args.channels = {0, 1}; // 2-channel synchronous reception (B210: 2 RX via one AD9361)
uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

// External clock & nanosecond time alignment
usrp->set_clock_source("external"); // 10 MHz reference
usrp->set_time_source("external");  // 1 PPS pulse
usrp->set_time_next_pps(uhd::time_spec_t(0.0));

// Timed command scheduling
usrp->set_command_time(usrp->get_time_now() + uhd::time_spec_t(0.05));
usrp->set_rx_freq(target_freq);
usrp->clear_command_time();
```

High-throughput driver optimizations:
- **UHD's asynchronous USB 3.0 transport** streams bulk-transfer packets directly into user-space
  host ring buffers, avoiding an extra memory copy through a generic OS socket layer.
- **Overflow handling**: single-producer single-consumer (SPSC) lock-free ring queues
  (`boost::lockfree::spsc_queue` or a custom cache-padded implementation, Section 10.1) prevent
  ring-buffer overflow (`O`) drops.
- **RFNoC 4.0 graph assembly**: the host builds dynamic FPGA processing graphs
  (`uhd::rfnoc::rfnoc_graph`), wiring DDC and FFT blocks with `graph->connect()`. Offloading 1D
  range FFTs to the FPGA reduces host-bound traffic from raw complex-IQ streams down to sparse
  point-cloud data, keeping well within USB 3.0's throughput budget.

A production zero-copy driver wraps `uhd::usrp::multi_usrp` and `uhd::rx_streamer` over UHD's USB 3.0
transport, pins the ingestion thread to an isolated CPU core with `SCHED_FIFO` real-time priority, and
allocates 64-byte cache-aligned frame structures:

```cpp
struct DriverConfig {
    std::string device_args = "type=b200"; // USRP B210, USB 3.0
    std::string subdev = "A:A A:B";        // single AD9361, 2x2 (2 TX, 2 RX)
    std::string clock_source = "external", time_source = "external";
    double center_freq_hz = 24.0e9, sample_rate_hz = 20.0e6, master_clock_rate = 56.0e6;
    double gain_db = 30.0;
    std::vector<size_t> rx_channels = {0, 1};
    size_t samples_per_buffer = 4096, cpu_core_id = 2;
    bool enable_simulation = false; // falls back to a synthetic FMCW generator if hardware is absent
};

class UhdZeroCopyDriver {
public:
    bool initialize();       // configures clock/time source, subdev, per-channel rate/freq/gain,
                              // builds the rx_streamer over the USB 3.0 transport
    bool start_streaming();  // pins thread affinity + SCHED_FIFO priority 99, issues STREAM_MODE_START_CONTINUOUS
    void stop_streaming();
    bool receive_frame(ZeroCopyFrame& frame, double timeout_sec = 0.1); // rx_streamer_->recv(...),
                              // tracks ERROR_CODE_OVERFLOW separately from other errors
};
```

Thread pinning uses `pthread_setaffinity_np` + `SCHED_FIFO` on Linux (and the Mach thread-affinity
API on macOS) to keep the ingestion thread on a dedicated, isolated CPU core.

---

## 10. Host Streaming, IPC & Visualization

### 10.1 Lock-free SPSC ring buffer

Single-producer single-consumer queue template using `alignas(64)` cache-line padding on `head_`
and `tail_` counters to avoid CPU false sharing, power-of-two capacity masking to replace modulo with
a bitwise AND, and atomic acquire/release ordering with no OS mutex:

```cpp
template<typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2!");
public:
    template<typename... Args> bool try_emplace(Args&&... args) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        if ((head - tail) >= Capacity) return false; // full
        new (&buffer_[(head & mask_)]) T(std::forward<Args>(args)...);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }
    bool try_dequeue(T& result) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) return false; // empty
        T* slot = &buffer_[(tail & mask_)];
        result = std::move(*slot); slot->~T();
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
private:
    static constexpr size_t mask_ = Capacity - 1;
    alignas(alignof(T)) char buffer_[Capacity * sizeof(T)];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};
```

### 10.2 ZeroMQ IPC binary protocol

Distributes range-Doppler matrices, CFAR point-cloud hits, raw IQ, and EKF track vectors over
non-blocking Unix domain sockets (`ipc:///tmp/radar_stream.ipc`) as binary multi-part ZeroMQ frames
with a fixed POD header:

```cpp
constexpr uint32_t RADAR_IPC_MAGIC = 0x52414441; // "RADA"

enum class MessageType : uint16_t {
    RAW_IQ_FRAME = 0x0001, RANGE_DOPPLER_MAP = 0x0002,
    CFAR_HIT_LIST = 0x0003, TRACK_STATE_LIST = 0x0004
};

#pragma pack(push, 1)
struct IpcHeader {
    uint32_t magic; uint16_t msg_type; uint16_t reserved;
    uint64_t frame_index; uint64_t timestamp_ns; uint32_t payload_bytes;
};
struct CfarHitPoint {
    uint16_t range_bin, doppler_bin;
    float range_m, velocity_m_s, snr_db, azimuth_deg, elevation_deg;
};
struct TrackStatePOD {
    uint32_t track_id; float x, y, vx, vy; float covariance[4][4];
};
#pragma pack(pop)

// RadarIpcPublisher: publish_rd_matrix(), publish_cfar_hits(), publish_raw_iq(), publish_tracks()
// RadarIpcSubscriber: receive() + matching unpack_*() static helpers, topic-filtered subscription
```

### 10.3 Visualization

| Feature | Qwt (Qt Widgets) | PyQtGraph (Python/Qt) | OpenGL / Dear ImGui (C++) |
|---|---|---|---|
| Language | C++ | Python | C++ |
| Rendering | CPU painter | PyOpenGL / Qt Graphics | Hardware OpenGL / Vulkan shaders |
| Max frame rate (512x512) | ~30 FPS | ~60 FPS | 120+ FPS |
| Host CPU overhead | High (CPU rasterization) | Medium (Python GIL) | Minimal (<2% CPU) |
| Dynamic range scaling | Manual palette lookup | Built-in colormaps | GLSL fragment-shader log scaling |
| Best fit | Legacy desktop GUIs | Rapid Python prototyping | High-performance production displays |

Range-Doppler matrices uploaded directly to GPU VRAM (`glTexSubImage2D`) are rendered via a GLSL
fragment shader that performs log-scaling and colormapping entirely on the GPU:

```glsl
// GLSL fragment shader for range-Doppler heatmaps
uniform sampler2D u_range_doppler_matrix;
uniform float u_dynamic_range_db;
in vec2 v_tex_coord;
out vec4 frag_color;

void main() {
    float power_linear = texture(u_range_doppler_matrix, v_tex_coord).r;
    float power_db = 10.0 * log10(power_linear + 1e-12);
    float norm_val = clamp((power_db + u_dynamic_range_db) / u_dynamic_range_db, 0.0, 1.0);
    frag_color = vec4(colormap_viridis(norm_val), 1.0);
}
```

### 10.4 GNU Radio radar flowgraph pattern

```
+--------------------------+       +-------------------------+       +---------------------------+
| signal_generator_fmcw_c  |------>| USRP TX / RX Front End  |------>| ts_fft_cc (Range FFT)     |
+--------------------------+       +-------------------------+       +---------------------------+
                                                                                    |
+--------------------------+       +-------------------------+                      v
| PyQtGraph Visualizer GUI |<------| estimator_fmcw / peak   |<------| 2D Matrix / Doppler FFT   |
+--------------------------+       +-------------------------+       +---------------------------+
```

Two mechanisms carry radar-specific data through GNU Radio flowgraphs:
1. **Tagged streams** — samples tagged with packet boundaries (`packet_len`, `rx_time`), letting
   blocks like `ts_fft_cc` buffer a complete coherent processing interval (CPI) before running 1D/2D
   FFTs.
2. **PMT message passing** — polymorphic types (`pmt::mp()`) carry low-rate detection dictionaries
   (`range`, `velocity`, `power`, `timestamp`) asynchronously from peak detectors
   (`find_max_peak_c`) to sinks, without blocking the sample-streaming queues.

---

## 11. Advanced / Experimental Techniques

Four autonomous/advanced extensions that layer on top of the classical FMCW/MIMO pipeline (Section
8) once it is working, targeting the host GPU/CPU rather than the FPGA.

### 11.1 Cognitive waveforms & active RF interference suppression

Mutual FMCW interference from uncooperative radars sharing the band corrupts short fast-time
intervals, raising the range-Doppler noise floor by 15-30 dB. Corrupted-sample detection uses Median
Absolute Deviation (MAD) envelope thresholding:

```
T_thresh = median(|r|) + gamma * MAD(|r|)
```

Corrupted samples form an interference subspace `U_int` via SVD; an orthogonal projection removes
it: `P_int_perp = I - U_int*U_int^H`, `r_clean = P_int_perp * r`. For multi-channel arrays, MVDR
adaptive beamforming applies spatial nulling toward the interference direction:
`w_opt = (R_xx^{-1} * a(theta_target)) / (a^H(theta_target) * R_xx^{-1} * a(theta_target))`.

```python
class CognitiveWaveformEngine:
    def suppress_subspace(self, r_raw, gamma=3.5):
        env = np.abs(r_raw)
        med = np.median(env)
        mad = np.median(np.abs(env - med))
        threshold = med + gamma * mad
        corrupt_idx = np.where(env > threshold)[0]

        r_clean = r_raw.copy()
        if len(corrupt_idx) > 0:
            w = np.ones(len(r_raw), dtype=float)
            w[corrupt_idx] = 0.0
            w = scipy.signal.medfilt(w, kernel_size=9)   # smooth the suppression mask
            r_clean = r_raw * w
        return r_clean, corrupt_idx, threshold
```

### 11.2 AI micro-Doppler target classification

Targets with internal kinematics (rotating drone blades, walking human limbs) induce micro-Doppler
phase modulation `f_micro(t) = (2*v0/lambda) + (2*L*Omega/lambda) * cos(Omega*t + phi0)`. A
128-point STFT produces log-compressed 128x128 spectrogram images, classified by a lightweight CNN
trained with focal loss into Drone / Human / Vehicle classes:

```
L_focal = -sum_c( alpha_c * (1 - p_hat(c))^gamma * y_true_c * log(p_hat(c)) )
```

```python
class MicroDopplerCNN(nn.Module):
    def __init__(self, num_classes=3):
        super().__init__()
        self.conv1 = nn.Conv2d(1, 16, 3, padding=1);  self.bn1 = nn.BatchNorm2d(16)
        self.pool  = nn.MaxPool2d(2, 2)
        self.conv2 = nn.Conv2d(16, 32, 3, padding=1); self.bn2 = nn.BatchNorm2d(32)
        self.fc1   = nn.Linear(32 * 32 * 24, 64)
        self.fc2   = nn.Linear(64, num_classes)

    def forward(self, x):
        x = self.pool(F.relu(self.bn1(self.conv1(x))))
        x = self.pool(F.relu(self.bn2(self.conv2(x))))
        x = x.view(x.size(0), -1)
        return self.fc2(F.relu(self.fc1(x)))
```

Trained models export to ONNX and run on the host GPU via ONNX Runtime / NVIDIA TensorRT (FP16) for
low-latency inference alongside the rest of the pipeline.

### 11.3 Compressed sensing for sparse radar recovery

Radar scenes are typically sparse in physical space (K << N targets). Compressed sensing collects
`M << N` non-uniform undersampled measurements (up to 75% data reduction):
`y = Phi*x + n = R_Omega * F^{-1} * x + n`. Orthogonal Matching Pursuit (OMP) reconstructs `x`
iteratively: (1) find the column with max correlation to the residual,
`lambda_k = argmax_j |phi_j^H * r_{k-1}|`; (2) add it to the active set; (3) least-squares solve on
the active columns; (4) update the residual; repeat until convergence.

| Feature | Standard 2D FFT | Compressed sensing (OMP/FISTA) | Advantage |
|---|---|---|---|
| Sampling constraint | Strict uniform Nyquist | Undersampled / non-uniform random | 50-75% data reduction |
| Range/Doppler resolution | Rayleigh-bound (1/T) | Sub-Rayleigh super-resolution | Resolves targets spaced <1/T |
| Sidelobe level | Window-dependent (-13 to -58 dB) | Near zero (sparse recovery) | Eliminates false-target masking |
| Computational load | O(N log N), fast | O(K*M*N), iterative | Suits CUDA GPU parallelism |

```python
def orthogonal_matching_pursuit(y, Phi, K_max=5, tol=1e-4):
    M, N = Phi.shape
    residual = y.copy(); selected_indices = []; x_rec = np.zeros(N, dtype=complex)
    for k in range(K_max):
        correlations = np.abs(Phi.conj().T @ residual)
        for idx in selected_indices: correlations[idx] = 0.0
        best_idx = np.argmax(correlations); selected_indices.append(best_idx)

        Phi_sub = Phi[:, selected_indices]
        x_sub, *_ = np.linalg.lstsq(Phi_sub, y, rcond=None)
        residual = y - Phi_sub @ x_sub
        if np.linalg.norm(residual) < tol: break

    for idx, val in zip(selected_indices, x_sub): x_rec[idx] = val
    return x_rec, selected_indices
```

### 11.4 Multi-static wireless clock & phase synchronization

Independent local oscillators across distributed multi-static nodes introduce carrier-frequency
offset, clock drift, and phase noise:
`theta_ij(t) = 2*pi*(f0_i - f0_j)*t + 2*pi*f0*(dt_i - dt_j) + (phi0_i - phi0_j)`.
Over-the-air (OTA) direct line-of-sight reference pulses measure the phase error over a known
stationary baseline `d_ij`:
`theta_ij_hat(t) = phi_LOS_measured_ij - 2*pi*f0*d_ij/c`,
`Omega_sync_ij = exp(-j * theta_ij_hat(t))`.
Applying `Omega_sync_ij` across distributed receiver signals restores spatial coherence for
sub-decimeter target multilateration.

---

## 12. Patent-Derived Hardware & RF Improvements

Concrete design improvements for this project's actual antenna and RF hardware — the 2 Tx x 2 Rx
5.8 GHz circularly-polarized array, its TX-RX isolation budget, and the board-level mechanical and
manufacturing choices around it — extracted from patent research (200+ radar and antenna patents,
2020-2024). This section is distinct in kind from Sections 1-11: those cover the software/FPGA
processing chain around a fixed RF front-end (the B210's integrated AD9361), while this section
covers the physical antenna array, feed network, and TX-RX isolation hardware that sits in front of
it. Baseline figures throughout are measured/simulated values from the current board revision;
"with improvements" figures are patent-derived projections, not yet measured on this hardware.

### 12.1 Virtual array optimization

**Non-uniform element spacing.** The current array uses uniform half-wavelength spacing: 4 virtual
elements at `(0,0), (lambda/2,0), (0,lambda/2), (lambda/2,lambda/2)`, lambda/2 = 25.844 mm at
5.8 GHz. Patent designs use non-uniform spacing to suppress grating lobes and improve angular
resolution:

```
Current (2 Tx x 2 Rx, 4 virtual elements): [0, lambda/2] x [0, lambda/2]
Upgrade path (3 Tx x 3 Rx, 9 virtual elements): [0, lambda/2, lambda/4] x [0, lambda/2, 3*lambda/4]
```

The 9-element non-uniform layout eliminates grating lobes near +/-45 degrees and improves
direction-of-arrival estimation by roughly 2-3x for the same physical aperture, at the cost of
needing 3 Tx and 3 Rx channels (a hardware upgrade beyond the current 2x2 AD9361-based front end)
and a more involved per-element calibration.

**TDM switching timing.** Fixed inter-chirp TX switching delay introduces a Doppler phase error of
`phi = 4*pi*v*tau/lambda` (this is the same error corrected in Section 8.1's Doppler phase
compensation, restated here as a hardware timing constraint rather than a software correction). At
v = 20 m/s and lambda = 0.0517 m, a 1 us switching delay produces 5.0 degrees of phase error per
microsecond. Keeping the switching delay under 0.5 us bounds this error to under 2.5 degrees before
Section 8.1's correction is even applied, so the two measures are complementary: hardware timing
discipline reduces the error the software then removes.

**Frequency hopping.** Hopping the carrier within the ISM band — 5.750, 5.800, 5.850 GHz, with
10-100 ms dwell time per frequency, pseudo-random or swept pattern — avoids fixed-frequency
interference from other radars and Wi-Fi while retaining the full 56 MHz bandwidth per hop. This
needs faster frequency synthesis and more complex signal processing than the fixed-frequency
baseline, but requires no antenna hardware change.

### 12.2 Circular polarization enhancements

**Current state.** The branch-line (3 dB hybrid) coupler feeds the CP patches with series arms at
35.36 ohm and shunt arms at 50 ohm, nominally producing a 90-degree phase split. Measured behavior:
the physical ring behaves about 10% electrically shorter than its drawn length, and a single element
measures 3.6 dB axial ratio due to the feed network loading its two orthogonal resonances unequally.

**Unequal power split.** Deliberately unbalancing the coupler — feeding 2.8 dB more power to the
lightly loaded resonance — cancels this unequal loading rather than fighting it. Simulated axial
ratio improves from 3.6 dB (lone element, unequal loading uncorrected) to 0.7 dB with the 2.8 dB
split, and from the array's measured 2.8-3.2 dB to under 1.5 dB when combined with neighbor-coupling
compensation. This also holds CP over a wider bandwidth than the equal-split design.

**Corner compensation in the coupler ring.** Each corner junction's true electrical reference point
sits inside the drawn corner, not at the drawn corner itself. Reducing each ring dimension by about
10% from the drawn (nominal) length compensates for this and re-centers the coupler at 5.8 GHz
instead of the 6.38 GHz the drawn dimensions would produce — consistent with the "~10% electrically
shorter" behavior already observed on the current board, and gives a concrete correction factor to
apply on the next revision rather than trial-and-error re-tuning.

**Dual circular polarization.** Adding a second input port to the coupler, with port selection
choosing RHCP or LHCP output, gives polarization flexibility for testing and for any future
bidirectional-communication use of the array. This is a feed-network addition, not a patch change.

### 12.3 TX-RX isolation and RF-domain leakage cancellation

This is the highest-priority hardware improvement in this document: it gates how much transmit power
the system can safely run, which in turn gates detection range more than any other single change
covered here.

**Current isolation budget.** Measured TX-RX leakage is -40.3 dB (worst case). The B210's RX
compression point is -15 dBm and its maximum safe input is 0 dBm. At +30 dBm TX power, -40.3 dB
isolation puts -30.3 dBm at the RX input (safe); at +33 dBm TX, that becomes -27.3 dBm (into
compression risk). This isolation figure is therefore the binding constraint on transmit power, not
the B210's own output rating.

**RF-domain leakage cancellation circuit.** The technique: sample the TX signal, generate an
amplitude- and phase-adjustable replica of the leakage path, and subtract that replica from the RX
signal in the RF domain before it reaches the LNA — rather than trying to remove leakage
digitally after the fact, where it has already consumed ADC dynamic range.

```
TX Path:
  Chirp Generator -> TX IQ Mixer -> PA -> TX Antenna
                          |
                     Reference Signal (sampled)
                          |
RX Path:
  RX Antenna -> Combiner (subtractor) -> LNA -> B210 RX
                     ^
              Replica IQ Mixer <- Reference Signal
              (amplitude/phase set by DAC, polarity-inverted)
```

Block diagram, drawn as signal flow:

```
+----------------+     +----------------+     +----------------+
|  Chirp         |     |  TX IQ Mixer   |     |     PA         |
|  Generator     |---->|  (frequency    |---->|                |----> TX Antenna
|                |     |   offset)      |     |                |
+----------------+     +----------------+     +----------------+
                       | reference signal
                       v
+----------------+     +----------------+
|  Replica IQ    |<----|  DAC (ampl.    |
|  Mixer         |     |  & phase)      |
|  (inverted)    |     +----------------+
+----------------+
      |
      v
+----------------+     +----------------+
|  Combiner      |<----|  RX Antenna    |
|  (subtractor)  |     +----------------+
+----------------+
      |
      v
+----------------+     +----------------+
|  LNA           |---->|  B210 RX       |
+----------------+     +----------------+
```

Component specifications:

| Component | Part / spec | Function |
|---|---|---|
| TX IQ mixer | HMC506 or equivalent, 5-6 GHz | Frequency-offsets the sampled TX reference for coherent replica generation |
| Replica IQ mixer | HMC506 or equivalent, 5-6 GHz | Produces the polarity-inverted leakage replica from the reference signal |
| DAC | 12- to 16-bit, 100 MS/s (e.g. AD5686-class) | Sets replica amplitude and phase |
| Combiner | HMC322 or equivalent, 90-degree hybrid | Subtracts the replica from the RX antenna signal ahead of the LNA |
| Control | FPGA or microcontroller | Drives the DAC from the calibration/tracking loop |

Calibration procedure:

1. Transmit a known signal with no target present (open range or absorber-terminated test setup).
2. Measure the residual leakage at the RX input.
3. Adjust the replica IQ mixer's amplitude and phase (via the DAC) to minimize the leakage FFT bin.
4. Fine-tune phase for correct polarity inversion (180 degrees relative to the true leakage path).

Expected performance: 40-50 dB cancellation depth, over the full 56 MHz bandwidth the B210 already
uses, with under 1 dB variation over temperature. Implementation footprint is small: two IQ mixers
plus a DAC and control logic, roughly 20x20 mm of additional PCB area, under 100 mW power, and
roughly $20-50 in components.

Circuit sketch for the next board revision (component/connection list, not a full schematic):

```
# Components
IQ_Mixer_TX:       HMC506
IQ_Mixer_Replica:  HMC506
Combiner:          HMC322
DAC:               AD5686 (16-bit, 100 MS/s)

# Connections
TX_Chirp -> IQ_Mixer_TX.IN
IQ_Mixer_TX.OUT -> PA.IN
PA.OUT -> TX_Antenna

TX_Chirp -> IQ_Mixer_Replica.IN
DAC.A -> IQ_Mixer_Replica.I
DAC.B -> IQ_Mixer_Replica.Q
IQ_Mixer_Replica.OUT -> Combiner.IN2

RX_Antenna -> Combiner.IN1
Combiner.OUT -> LNA.IN
```

Calibration code (`calibrate_leakage_cancellation`, Python) — cross-correlates the sampled TX
reference against the RX signal to find the leakage path delay, then estimates the amplitude ratio
and phase offset the replica mixer needs:

```python
import numpy as np
from scipy import signal

def calibrate_leakage_cancellation(tx_signal, rx_signal, fs=61.44e6):
    """
    Calibrate leakage cancellation parameters from a no-target TX/RX capture.
    """
    # Cross-correlate to find delay
    corr = signal.correlate(rx_signal, tx_signal, mode='full')
    delay_samples = np.argmax(np.abs(corr)) - len(tx_signal)
    delay_sec = delay_samples / fs

    # Estimate amplitude and phase via analytic-signal cross term
    tx_analytic = signal.hilbert(tx_signal)
    rx_analytic = signal.hilbert(rx_signal)
    tx_aligned = np.roll(tx_analytic, -delay_samples)

    amplitude_ratio = np.abs(np.mean(rx_analytic * np.conj(tx_aligned)))
    phase_offset = np.angle(np.mean(rx_analytic * np.conj(tx_aligned)))

    return {'delay': delay_sec, 'amplitude': amplitude_ratio, 'phase': phase_offset}
```

**Adaptive leakage tracking.** A continuous variant of the same loop re-estimates the leakage from
the DC bin of the live RX spectrum and updates the replica DAC outputs in real time, compensating for
temperature drift and component aging without a full recalibration cycle:

```python
# Continuous adaptive control loop
leakage_estimate = fft(rx_signal)[0]              # DC bin
replica_amplitude = abs(leakage_estimate)
replica_phase = angle(leakage_estimate) + 180      # invert

dac_amplitude.write(replica_amplitude)
dac_phase.write(replica_phase)
```

Expected tracking performance: under 100 us update latency, under 0.1 dB / 1 degree steady-state
accuracy, and 30 dB of dynamic range over which the loop can keep the cancellation locked. This is a
higher-complexity addition than the static calibration above and is worth deferring until the static
version is proven on hardware.

**Orthogonal feeding for additional isolation.** The array's existing polarization scheme is RHCP
TX / LHCP RX (circular orthogonal). Adding linear-polarization orthogonality on top — TX elements fed
vertically, RX elements fed horizontally, in addition to the existing CP sense — is a simple
feed-orientation change that adds an estimated 10-15 dB of isolation, for a combined total above
50 dB when stacked with the CP isolation already in place. The trade-off is a somewhat more complex
feed network and the need to re-verify polarization purity after the change.

### 12.4 Antenna array and mechanical improvements

**Ground plane extension.** Currently 25 mm past the patch edge (lambda/4 at 5.8 GHz), measured at
2.74 dB axial ratio versus 7.61 dB at a 6 mm extension — confirming that ground-plane extension is a
major axial-ratio lever on this design. Patent data indicates 25 mm is the minimum useful extension
and 30-40 mm is closer to optimal, trading off against board size. Recommended action: verify axial
ratio at the field edges with the current 25 mm extension, and extend to 30 mm if it measures above
3 dB there; a tapered ground plane is worth considering if board area is tight.

**Mutual coupling mitigation.** At the array's lambda/2 spacing (25.844 mm), neighbor coupling
degrades a single element's 0.7 dB axial ratio to 3-7 dB in the array context (the second element is
already mirrored to partially compensate). Two further techniques from the patent literature:

- **Electromagnetic bandgap (EBG) structures** — a periodic pattern (square patches with gaps,
  roughly lambda/10 = 5.17 mm dimensions and spacing) added to the ground plane between elements,
  suppressing the surface waves that carry mutual coupling. Reduces mutual coupling by an estimated
  5-10 dB with minimal impact on the radiation pattern, at the cost of a more complex ground plane
  and slightly larger board area.
- **Defected ground structure (DGS)** — dumbbell-shaped slots (roughly 10x2 mm, 1 mm gap) etched
  into the ground plane between elements, creating a stopband that suppresses coupling specifically
  at 5.8 GHz and can also improve bandwidth.

**TX-RX mechanical separation.** Currently 250 mm, giving 25.8 dB of free-space path loss between
the TX and RX apertures. Patent-derived analysis puts the useful range at 220-280 mm (24.6 dB to
26.8 dB loss), with diminishing returns above 300 mm (under 0.5 dB gained per additional 10 mm). The
current 250 mm is within the optimal range; keeping the mechanical design adjustable across
220-280 mm preserves flexibility, but no change is needed on the current board.

### 12.5 Beamforming and direction-of-arrival improvements

**Multi-point calibration.** The current calibration is single-point (boresight only). Measuring
phase and amplitude for all 4 virtual elements at multiple angles across the field of view —
-45, -30, -15, 0, 15, 30, 45 degrees, with a corner reflector — and storing the result for use in the
processing chain corrects an observed 8.5-degree phase error at +/-20 degrees and a 10.5-degree phase
error at +/-45 degrees, improving angle accuracy from roughly +/-5 degrees to +/-2 degrees. This
needs no new hardware — only the existing calibration setup, one to two hours of measurement, and a
day or two of software to consume the resulting per-angle calibration table (e.g. stored as a JSON
file keyed by angle, one entry per steering direction with phase/amplitude per virtual element):

```python
import json

def multi_point_calibration(angles, measurements):
    """
    Build a per-angle calibration table from corner-reflector measurements.
    measurements[i]: dict with 'phases' and 'amplitudes' for each virtual element.
    """
    calibration = {}
    for angle, measurement in zip(angles, measurements):
        calibration[str(angle)] = {
            'phases': measurement['phases'],
            'amplitudes': measurement['amplitudes'],
            'timestamp': measurement.get('timestamp', ''),
        }
    with open('calibration_data.json', 'w') as f:
        json.dump(calibration, f, indent=2)
    return calibration
```

**Capon beamformer.** Section 8.2 already covers the Capon/MVDR formulation in full —
`P_Capon(theta) = 1/(a^H(theta) * R_xx^{-1} * a(theta))`, with diagonal loading `R_dl = R_xx +
gamma*I` — as one of the four AoA options for this array, alongside phase monopulse, Bartlett, and
2D MUSIC, and gives its complexity and resolution trade-offs against those alternatives (Section 8.2
trade-off table). The patent research reaches the same technique from the antenna-array-improvement
side and adds a steering vector specific to this board's actual geometry (the 250 mm TX-RX
separation, not just an abstract half-wavelength spacing) worth keeping alongside Section 8.2's more
general derivation:

```python
import numpy as np

def capon_beamformer(R, a, theta_scan):
    """
    Capon (MVDR) beamformer for DoA estimation — see Section 8.2 for the general
    formulation and its comparison against phase monopulse / Bartlett / 2D MUSIC.

    R : ndarray (4,4) - sample covariance matrix
    a : callable      - steering vector function of angle
    theta_scan : ndarray - scan angles
    """
    P = np.zeros(len(theta_scan))
    w = np.zeros(4, dtype=complex)
    for i, theta in enumerate(theta_scan):
        a_theta = a(theta)
        R_inv = np.linalg.inv(R)
        w = R_inv @ a_theta / (a_theta.conj().T @ R_inv @ a_theta)
        P[i] = 1 / (a_theta.conj().T @ R_inv @ a_theta)
    return P, w

def steering_vector(theta):
    """Steering vector for this board's 2x2 virtual array (250 mm TX-RX separation)."""
    lambda_ = 0.0517   # wavelength at 5.8 GHz
    d = 0.25           # TX-RX separation (250 mm, this board)
    x = np.array([0, 0, d, d])
    y = np.array([0, d, 0, d])
    return np.exp(1j * 2 * np.pi / lambda_ * (x * np.sin(theta) + y * np.cos(theta)))
```

Measured against this array's current FFT-based estimator (6-degree resolution, -13 dB sidelobes),
Capon is expected to deliver 2-3-degree resolution and -20 dB sidelobes at roughly 10x the
computation — consistent with Section 8.2's "medium" complexity rating for Capon versus FFT/Bartlett.

**Nested array (future upgrade).** Moving to 3 Tx / 3 Rx with non-uniform positions —
`[0, lambda/2, lambda/4]` for Tx, `[0, lambda/2, 3*lambda/4]` for Rx — produces 9 virtual elements
(2.25x the current 4) with non-uniform spacing, giving 1.5-2x better resolution and eliminating
grating lobes (this is the same array geometry described in Section 12.1). This requires a hardware
upgrade beyond the current 2x2 AD9361 front end and more involved calibration, so it belongs after
the isolation and calibration work above rather than before it.

### 12.6 Drone-detection signal processing

**Micro-Doppler classification.** Rotor blades produce periodic Doppler modulation distinct from a
drone's bulk-motion Doppler shift. Section 11.2 already covers the general micro-Doppler CNN
classification pipeline for this system (STFT spectrogram plus a lightweight CNN, Drone/Human/Vehicle
classes). The patent-derived addition here is a concrete drone-specific frequency table to seed that
classifier's target classes, and a feature-extraction routine that reads directly off the STFT rather
than requiring a full CNN forward pass for a coarse rotor-frequency classification:

| Drone type | Rotor frequency (Hz) | Blade count | Micro-Doppler bandwidth (Hz) | Signature |
|---|---|---|---|---|
| DJI Mavic Air-class quadcopter | 25-35 | 4 | 100-140 | Strong 4th harmonic |
| DJI Phantom 4-class quadcopter | 20-30 | 4 | 80-120 | Symmetric pattern |
| DJI Inspire 2-class quadcopter | 15-25 | 4 | 60-100 | Dual frequency (front/rear rotor pairs) |
| Fixed-wing | 5-15 | 2 | 20-60 | Weak modulation |
| Racing drone | 40-60 | 4 | 160-240 | High frequency |
| Helicopter | 5-10 | 2-4 | 20-80 | Complex, asymmetric |

```python
def extract_micro_doppler_features(signal, fs=61.44e6):
    """
    Extract micro-Doppler features from an FMCW radar signal for coarse rotor
    classification, ahead of / alongside the CNN classifier in Section 11.2.
    """
    f, t, Zxx = scipy.signal.stft(signal, fs, nperseg=1024)

    features = {}
    features['fundamental'] = detect_fundamental(Zxx, f)
    features['harmonics'] = detect_harmonics(Zxx, f)
    features['modulation_depth'] = calculate_modulation_depth(Zxx)
    features['symmetry'] = calculate_symmetry(Zxx)
    return features
```

Classification accuracy above 90% and a 50-80% reduction in false alarms are the reported figures
for micro-Doppler-based classification versus range-Doppler detection alone.

**Multi-band detection.** Monitoring 2.4 GHz (drone control link) alongside the existing 5.8 GHz
band (drone video link) — via a second B210 or by frequency-hopping a single one — raises detection
probability from roughly 70% (5.8 GHz alone) to above 95%, and additionally distinguishes control
from video transmission as a classification feature:

| Band | Purpose | Typical power | Modulation | Detection range |
|---|---|---|---|---|
| 2.4 GHz | Control | 10-100 mW | FHSS, DSSS | 500-1000 m |
| 5.8 GHz | Video | 25-1000 mW | OFDM | 1000-3000 m |

**Clutter suppression.** Two techniques, both implementable directly in the existing range-Doppler
processing chain without new hardware:

- **Ground clutter mitigation** — estimate the static-clutter floor via a median filter across
  time on the range profile, then subtract it from the current profile. Reported clutter suppression
  is 30-40 dB, extending usable detection down toward 0-degree elevation for low-altitude drones.
- **Moving target indication (MTI)** — a Doppler-domain velocity threshold (e.g. 1 m/s) masks out
  near-zero-velocity bins, rejecting stationary returns by over 40 dB while preserving sensitivity to
  moving targets. This is the simplest of the two to add to the existing pipeline.

### 12.7 Materials and manufacturing

**Substrate surface finish.** Current board uses ZYF300CA-P (er = 3.00, tan-delta = 0.0018) and
RO4350B (er = 3.48, tan-delta = 0.0037) with an immersion-silver finish.

| Finish | Skin depth at 5.8 GHz | Resistance ratio | Range impact | Cost |
|---|---|---|---|---|
| Bare copper | N/A | 1.0x | Baseline | Low |
| ENIG (nickel) | 3 um | 2.7x | -4.3% range | Medium |
| Immersion silver (current) | <0.1 um | 1.0x | 0% range | Medium |
| Immersion gold | <0.1 um | 1.0x | 0% range | High |

The current immersion-silver finish is already the correct choice on RF-loss grounds; ENIG carries a
4.3% range penalty from its higher skin-effect resistance and should be avoided on this design.
Immersion gold is worth considering only if harsh-environment durability becomes a requirement, since
it offers no RF advantage over silver.

**Copper roughness.** Standard FR-4-grade copper (Ra 1.5-3.0 um) adds 10-20% conductor loss; low
profile copper (Ra under 0.5 um) keeps that under 5%; reverse-treat foil (Ra under 0.2 um) keeps it
under 2%. Specifying reverse-treat foil or low-profile copper on the next board revision is estimated
to be worth a 5-10% range extension for no design change beyond the fab spec.

**Etching tolerance.** Standard etching tolerance (+/-0.05 mm) produces +/-2-3 ohm variation on a
50 ohm line and +/-5 degrees of phase error on coupler dimensions — directly relevant to the
branch-line coupler tuning in Section 12.2. Tightening the tolerance to +/-0.02 mm on the coupler
ring, patch size, and feed line widths is estimated to be worth about 0.5 dB of axial ratio
improvement.

**Dielectric constant tolerance.** ZYF300CA-P is specified at er = 3.00 +/- 0.05, which translates to
+/-0.85% variation in wavelength and therefore in patch size. Designing for the upper bound (er =
3.05) and verifying at the lower bound (er = 2.95) is estimated to be worth about 0.3 dB of match
improvement over designing to the nominal value alone.

### 12.8 Power and range enhancements

**Transmit power scaling.** The isolation figure from Section 12.3 is the direct limiter on safe TX
power: `isolation required > P_TX - P_RX_max`. With B210 P_RX_max = -15 dBm and the current -40.3 dB
isolation, the maximum LNA gain that keeps the RX chain safe is 25.3 dB (40.3 - 15). Each isolation
improvement raises how much TX power and LNA gain the system can run simultaneously:

| Configuration | Isolation gain | Max TX power | Detection range (0.01 sq m target) |
|---|---|---|---|
| Baseline | 0 dB | +10 dBm | 173 m |
| + RF cancellation (Section 12.3) | +20 dB | +30 dBm | 771 m |
| + RF cancellation + LNA (15 dB) | +20 dB | +30 dBm | 915 m |
| + RF cancellation + orthogonal feeding (Section 12.3) | +25 dB | +35 dBm | 1.2 km |

Implementation order follows directly from this table: RF cancellation first (771 m at +30 dBm),
then add the LNA (915 m), then orthogonal feeding if the extra isolation and TX headroom are needed
(1.2 km).

**LNA placement.** Moving the LNA as close to the antenna as practical — shortening the
antenna-to-LNA cable from roughly 0.5 m to 0.1 m, at the cost of a longer LNA-to-B210 run — improves
system noise figure from 1.54 dB to 1.45 dB, worth an estimated 2-3 m of additional range. Small on
its own, but free: it costs cable rearrangement, not new components.

**LNA gain selection.** With 15-20 dB LNA gain (safe margin under the 25.3 dB ceiling derived above),
range improves from 173 m to 250-350 m at the current isolation. Once RF cancellation (Section 12.3)
adds its 20 dB of isolation headroom, LNA gain up to 35-40 dB becomes viable, and the range moves
into the 350-400 m range at that intermediate power level, before the larger TX-power increases in
the table above are even applied.

### 12.9 Mechanical and thermal design

**PA thermal design (for a future PA board).** A 10x10 via grid under the PA die — 0.3 mm via
diameter, 1.0 mm pitch, 2 oz (70 um) copper, with a 5x5 mm heat sink pad under the PA — brings
thermal resistance down from about 5 C/W to 2 C/W, raising the sustainable continuous PA power from
1 W to 2 W and improving long-term reliability.

**Connector thermal relief.** Replacing a direct solder connection from SMA connector pads to the
ground plane with a thermal-relief pattern (four 0.5 mm spokes per pad) makes soldering easier to get
right (fewer cold joints) and reduces thermal-cycling stress on the connector over its service life.

**Board edge protection.** The current design already uses 3 mm rounded corners. Adding a 1 mm,
45-degree chamfer to the board edges on top of that is estimated to roughly double mechanical
edge strength and reduce chipping during assembly handling.

**Fiducial design.** Increasing fiducial marks from 1 mm copper in a 2 mm mask opening to 1.5 mm
copper in a 3 mm opening (with 1 mm clearance from other features) is estimated to improve
pick-and-place placement accuracy from +/-0.05 mm to +/-0.02 mm, which also feeds directly into the
etching-tolerance improvement in Section 12.7.

### 12.10 Performance summary: baseline versus with improvements

| Metric | Baseline (current board) | With all improvements in this section | Change | Primary driver |
|---|---|---|---|---|
| TX-RX isolation | -40.3 dB | -65 dB | +25 dB | RF cancellation (12.3) + orthogonal feeding (12.3) |
| TX power | +10 dBm | +35 dBm | +25 dB | Isolation headroom from the above |
| RX noise figure | 8.0 dB (B210 alone) | 1.4 dB | -6.6 dB | LNA + optimized placement (12.8) |
| Detection range (0.01 sq m target) | 173 m | 1.2 km | 7x | All RF power/isolation improvements combined |
| DoA resolution | ~6 degrees (FFT) | ~2 degrees | 3x | Capon beamformer + multi-point calibration (12.5) |
| Axial ratio | 2.8-3.2 dB | <1.5 dB | -1.5 dB | Unequal coupler split + calibration (12.2) |
| Drone classification accuracy | Not implemented | >90% | New capability | Micro-Doppler analysis (12.6) |
| False alarm rate | Baseline (FFT detection only) | <10% | -80% relative | Multi-band detection + classification (12.6) |

### 12.11 RF hardware improvement roadmap

This roadmap covers the antenna, feed network, isolation, and mechanical work in this section. It
runs in parallel with, and is largely independent of, the software/FPGA roadmap in Section 16 — the
two touch different parts of the system (RF hardware upstream of the AD9361 versus the FPGA/host
processing chain downstream of it) and can proceed on separate tracks.

**Phase 1 — measurement and low-risk software (1-2 weeks).** Multi-point calibration across the
field of view (Section 12.5, software only, no hardware change). Ground plane axial-ratio
verification at the current 25 mm extension, extending to 30 mm only if measurement shows it's
needed (Section 12.4). LNA placement optimization (Section 12.8, cable rearrangement only).

**Phase 2 — RF hardware and beamforming software (1-2 months).** RF-domain leakage cancellation
circuit (Section 12.3) — the highest-priority item in this section, since it gates every subsequent
power/range improvement. Capon beamformer software (Section 12.5, builds on the existing Section 8.2
AoA framework). Micro-Doppler classification (Section 12.6, extends the Section 11.2 CNN pipeline).

**Phase 3 — adaptive and multi-hardware extensions (3-6 months).** Adaptive leakage tracking
(Section 12.3), which turns the Phase 2 static calibration into a continuously-adjusting loop.
Dual-band detection (Section 12.6), which needs a second B210 or a frequency-hopping single-radio
scheme. Nested-array upgrade to 3x3 (Sections 12.1, 12.5), which is the only item in this roadmap
that requires a new antenna board revision rather than a firmware/software change to the existing
hardware.

---

## 13. FPGA Device Note

The B210's FPGA is fixed by the board — this is not a component-selection decision the way it would
be when designing a custom FPGA carrier. The confirmed hardware is a Kintex-7 device, which is why
RFNoC 4.0 and the Kintex-7 resource-budget figures throughout this catalog (Section 5.5) apply
directly. Those figures use the Kintex-7 XC7K325T as the representative baseline device, since it is
the part used across the Gen-3 USRP line and gives a realistic picture of the margin available on a
Kintex-7 fabric for the radar IP described here.

---

## 14. Processing Allocation Matrix

Every processing stage mapped to its execution target, with the reasoning behind the split.

| Stage | Target | Technology | Rationale | Bandwidth/latency impact |
|---|---|---|---|---|
| TDM pulse & switch timing | FPGA | Kintex-7 RTL state machine | Needs 5 ns deterministic timing; host OS introduces microsecond jitter | Zero network bandwidth; eliminates timing jitter |
| High-rate ADC sampling & DDC | FPGA | DSP48E1 / NCO | Mixes wideband RF/IF, decimates raw ADC stream to baseband | 32x bandwidth reduction (200 -> 6.25 MSps) |
| Windowing (Blackman-Harris) | FPGA | DSP48E1 inline multiplier | Suppresses range sidelobes (>58 dBc) at line rate | Zero added latency |
| Fast-time 1D range FFT | FPGA | LogiCORE / open FFT core | Continuous streaming 1024-pt FFT at up to 350 MHz | Converts time-domain beat samples to range spectrum |
| Corner-turn matrix transpose | FPGA | Ping-pong BRAM (114 tiles) | Transposes range-Doppler matrix entirely on-chip | Eliminates host memory-transfer latency |
| Slow-time 2D Doppler FFT | FPGA | LogiCORE / open FFT core | 128-pt FFT across chirps forms the range-Doppler map | Produces full range-Doppler map |
| TDM Doppler phase compensation | FPGA | CORDIC / DSP48 | Aligns V2/V3 channel phase at line rate | Prepares 4 virtual channels for spatial processing |
| 2D CA-CFAR detection | FPGA (or host) | HLS line buffer / integral image | BRAM sliding windows compute noise thresholds | >99% bandwidth reduction — sparse hit list instead of full matrix |
| Phase Monopulse AoA | FPGA | CORDIC vectoring | Instantaneous angle for coarse single hits | <20 clock cycles per hit |
| 2D MUSIC / Capon AoA | Host | Eigen3 / CUDA | 4x4 eigendecomposition + 2D grid search needs floating point | Receives sparse hit stream, computes super-resolution angles |
| Centroiding & DBSCAN | Host | C++17 | Merges multi-cell hits into point-cloud clusters | Negligible CPU load |
| EKF target tracking | Host | C++17 | Dynamic state estimation, Jacobian, track lifetime management | Low CPU load (~10 tracks) |
| Real-time visualizer GUI | Host GPU | OpenGL / Dear ImGui | Direct-to-VRAM textures, GLSL fragment-shader colormap | 120+ FPS at <2% host CPU |
| Doppler phase shift compensation (host copy) | Host | C++17 AVX-512 / Python | Applied to the full tensor when not done on FPGA | O(N_range * N_doppler * N_tx) |
| Cognitive RF subspace nulling | Host | NumPy / SVD | Interference detection and MVDR nulling | O(N_corrupt^3) |
| AI micro-Doppler CNN classifier | Host GPU | ONNX Runtime / TensorRT | CNN inference on spectrogram images | O(CNN forward pass) |
| Compressed sensing OMP solver | Host GPU/CPU | CUDA cuBLAS | Sparse recovery from undersampled data | O(K*M*N) |
| Multi-static coherent beamformer | Host CPU/GPU | NumPy matrix ops | Cross-node phase sync and localization | O(N_T * N_R * N_grid) |
| Binary IPC handoff & visualization transport | Host | C++17 ZeroMQ / OpenGL | Publishes matrices/hits/tracks to the GUI process | O(N_hits) |

---

## 15. System Performance Specifications

Representative full-system numbers for the 2x2 TDM-MIMO baseline described throughout this catalog:

| Parameter | Value | Basis |
|---|---|---|
| RF carrier frequency | 77.0 GHz / 24.0 GHz | Millimeter-wave FMCW automotive/defense band |
| Sweep bandwidth (B) | 1.0 GHz | Active LFM ramp bandwidth |
| FPGA system clock | 200.0 MHz (5.0 ns period) | Kintex-7 fabric clock |
| ADC raw sampling rate | 200.0 MSps | Dual 16-bit signed I/Q |
| CIC decimation factor | 8 (5-stage filter) | Multiplierless reduction to 25.0 MSps |
| Fast-time range bins | 1024 samples/chirp | Range resolution `dR = c/(2B) = 0.15 m` |
| Slow-time chirps | 128 chirps/CPI | Velocity resolution `dv = lambda/(2*N_c*T_PRT) = 0.234 m/s` |
| Pulse repetition time | 60.0 us (12,000 clk cycles) | 51.2 us sweep + 1.0 us settle + 7.8 us idle |
| Coherent processing interval | 7.68 ms (128 x 60 us) | Frame rate 120.0 FPS (8.33 ms budget) |
| Virtual array topology | 4-element ULA (2 Tx x 2 Rx) | lambda/2 spacing, 28.6-degree Rayleigh resolution |
| Super-resolution angular accuracy | <1.5 degrees az/el | 2D MUSIC eigen-subspace decomposition |
| Raw ADC data throughput | 6.4 Gbps (200 MSps x 32-bit) | Kintex-7 on-chip bus |
| Host USB 3.0 stream throughput | 100.0 MB/s (25 MSps decimated) | UHD USB 3.0 zero-copy transport |
| Hardware processing latency | <5.0 us | FPGA pipeline delay |
| End-to-end system latency | <8.33 ms | Full acquisition to 3D point-cloud render |

Technology stack: SystemVerilog (IEEE 1800-2012) / Verilog-2005 firmware; Xilinx Vivado 2022.2 or
2023.1 with Ettus RFNoC 4.0; C++17 (GCC 11+/Clang 13+) host driver over UHD's USB 3.0 transport with
POSIX real-time extensions; ZeroMQ 4.3.4 and VITA-49.0/49.2 CHDR transport for IPC; Eigen3 3.4+, FFTW3 or
Intel MKL, PyTorch 2.0+, ONNX Runtime 1.15+, TensorRT 8.6+ for host DSP/ML; Python 3.10+, NumPy
1.24+, SciPy 1.10+ for prototyping.

---

## 16. Implementation Roadmap

**Phase 1 — System design & simulation (months 1-2)**
Define radar specifications (frequency, bandwidth, range, resolution); size FFT/filter/resource
needs against the Kintex-7 budget (Section 5.5); confirm the USB 3.0 host interface (Section 9);
model algorithms in MATLAB/Simulink or GNU Radio before committing to RTL.

**Phase 2 — FPGA development (months 3-6)**
Install Vivado 2023.1+, UHD 4.0, GNU Radio 3.10+. Build the base system (DDR3 controller,
clocking/reset). Build UHD with RFNoC 4.0 support and validate the default FPGA image.
Develop custom IP: pulse compression, CFAR (CA/GO), beamforming, and integrate with RFNoC blocks.

**Phase 3 — Host software development (months 4-7, overlaps Phase 2)**
Install GNU Radio, SoapySDR, and a DSP library (KFR recommended). Implement the FMCW processing
chain and MIMO processing, wire in the custom FPGA IP, and build the real-time pipeline. Develop the
control application (Qt/PyQt), visualization (Matplotlib/VTK or custom OpenGL), and system
monitoring.

**Phase 4 — Integration & testing (months 7-9)**
Connect the RF frontend, validate ADC/DAC timing and synchronization, implement calibration.
Functional test every component, benchmark performance and latency, profile and optimize
bottlenecks, and harden error handling/recovery.

**Phase 5 — Deployment & documentation (months 9-12)**
Final assembly and stress/reliability testing, system architecture and API documentation,
operator training, and maintenance procedures.

Total: 9-12 months for a complete system, following this phased approach.

---

## 17. Cost Estimates

| Item | Cost range | Notes |
|---|---|---|
| Kintex-7 FPGA board (XC7K325T) | $200-$500 | Commercial board |
| Development kit | $1,000-$3,000 | Includes FPGA, memory, interfaces |
| RF frontend (integrated AD9361 transceiver on the B210: RF + ADC/DAC in one chip) | $500-$5,000 | Depends on frequency; B210-class boards already integrate this |
| Host PC | $1,000-$3,000 | High-performance workstation |
| Software tools | $0-$5,000 | Vivado free; MATLAB optional |
| **Total (estimate)** | **$2,700-$13,500** | Varies by configuration |

Open-source savings: SDR frameworks (GNU Radio, srsRAN, LimeSuite), DSP libraries (KFR, FFTW,
Armadillo), IP cores (Xilinx official + open-source alternatives), RFNoC (bundled with UHD), and
radar libraries (OpenRadar, RadarLib) are all free — an estimated $50,000-$200,000 saved versus
commissioning an equivalent commercial radar system.

---

## 18. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| FPGA resource limitations | High | Start with XC7K325T (Section 5.5 shows large surplus headroom); upgrade to XC7K410T if needed |
| USB 3.0 throughput/driver issues | Medium | Use UHD's tested USB 3.0 transport; validate sustained throughput margin under real streaming load |
| RFNoC learning curve | Medium | Follow Ettus Research tutorials and workshops before writing custom blocks |
| Real-time performance shortfalls | High | Profile early; optimize the critical paths first (FFT, pulse compression, CFAR) |
| Hardware compatibility gaps | High | Verify RF frontend specifications against the FPGA/ADC interface before integration |
| Licensing issues | Low | Prefer open-source or free Xilinx IP cores; check licenses in Sections 2-9 before depending on a component |
| Toolchain complexity | Medium | Use Docker containers for a consistent Vivado/UHD/GNU Radio build environment |
| Debugging difficulty | High | Build in comprehensive logging/monitoring (the ZeroMQ IPC layer in Section 10.2 doubles as an observability channel) |

---

## 19. Symbol Glossary

- `f0`: FMCW RF carrier center frequency (Hz)
- `B`: chirp linear-FM bandwidth (Hz)
- `K_slope`: FMCW chirp slope sweep rate (Hz/s)
- `T_chirp` / `T_PRT`: chirp ramp duration / pulse repetition time (s)
- `N_range` / `N_doppler`: number of fast-time range bins / slow-time Doppler chirps
- `R`: CIC filter decimation rate factor
- `I(r, m)`: 2D integral image (prefix-sum matrix) for O(1) CA-CFAR evaluation
- `alpha_CA` / `alpha_OS`: CFAR detection threshold scaling factors
- `R_xx`: spatial covariance matrix
- `U_s` / `U_n`: signal / noise subspace eigenvector matrices
- `a(theta, phi)`: virtual array spatial steering vector
- `P_MUSIC(theta, phi)`: 2D MUSIC super-resolution pseudospectrum power (dB)
- `Phi = R_Omega * F^{-1}`: compressed-sensing sub-Nyquist sensing matrix
- `P_int_perp`: orthogonal subspace projection matrix for RF interference nulling
- `theta_ij(t)`: distributed multi-static phase error model

---

## 20. References

Official documentation:
- Xilinx Kintex-7 FPGA Data Sheet (DS182)
- Xilinx Vivado Design Suite documentation
- Ettus Research UHD Manual (files.ettus.com/manual)
- Ettus Research RFNoC Documentation (kb.ettus.com/RFNoC)
- GNU Radio Wiki (wiki.gnuradio.org)
- LimeSuite / SoapySDR documentation

Open-source repositories: see the full catalog in Section 2, plus:
- github.com/gnuradio/gnuradio
- github.com/EttusResearch/uhd
- github.com/EttusResearch/rfnoc-oot-blocks
- github.com/kfrlib/kfr
- github.com/openXC7
- github.com/casper-astro (CASPER)

Community resources: GNU Radio Discuss mailing list and Discord; Ettus Research Forum; Lime
Microsystems Forum; Xilinx Forums; r/RFEngineering; r/DSP.

Research papers cited in Section 7, plus the following academic bibliography from the deep
architecture blueprint (Section 8-11 math):
1. Mark A. Richards, *Fundamentals of Radar Signal Processing*, 2nd ed., McGraw-Hill, 2014.
2. Merrill I. Skolnik, *Radar Handbook*, 3rd ed., McGraw-Hill, 2008.
3. R. O. Schmidt, "Multiple emitter location and signal parameter estimation," *IEEE Trans. Antennas
   Propag.*, vol. 34, no. 3, pp. 276-280, 1986.
4. E. B. Hogenauer, "An economical digital filter for decimation and interpolation," *IEEE Trans.
   Acoust., Speech, Signal Process.*, vol. 29, no. 2, pp. 155-162, 1981.
5. D. L. Donoho, "Compressed sensing," *IEEE Trans. Inf. Theory*, vol. 52, no. 4, pp. 1289-1306, 2006.
6. J. A. Tropp and A. C. Gilbert, "Signal recovery from random measurements via Orthogonal Matching
   Pursuit," *IEEE Trans. Inf. Theory*, vol. 53, no. 12, pp. 4655-4666, 2007.
7. V. C. Chen, *The Micro-Doppler Effect in Radar*, Artech House, 2011.
