#=============================================================================
# radar.xdc -- timing constraints for the radar core
#
# The core is entirely synchronous to radio_clk, which the b200 design derives
# from the AD9361 data clock.  At the 61.44 MHz master clock rate that is a
# 16.276 ns period, which is loose for a Kintex-7 -2 part: the tightest path in
# the design is the de-chirp complex multiply, and a DSP48E1 closes a 25x18
# multiply-accumulate well past 400 MHz.  The constraint exists so that a
# failure is reported rather than silently shipped, not because it is hard.
#
# The settings bus (set_stb / set_addr / set_data) is asynchronous to the
# datapath in the b200 design and is already synchronised upstream, so it is
# constrained as a false path here rather than being over-constrained.
#=============================================================================

#-----------------------------------------------------------------------------
# Primary clock.  When the core is built out of context this creates the clock;
# inside the b200 design radio_clk already exists, so the create_clock is
# guarded.
#-----------------------------------------------------------------------------
if {[llength [get_clocks -quiet radio_clk]] == 0} {
    create_clock -name radio_clk -period 16.276 [get_ports radio_clk]
}

#-----------------------------------------------------------------------------
# The core is a single clock domain.  Everything below is about the boundary.
#-----------------------------------------------------------------------------

# Settings bus: written by the host at human speed, synchronised upstream.
set_false_path -to [get_cells -hier -filter {NAME =~ *radar_regs*/reg_*}]

# Soft reset fans out very widely and has no timing requirement beyond being
# stable for more than a clock, which it is by construction.
set_false_path -from [get_cells -hier -filter {NAME =~ *soft_reset_reg*}]

#-----------------------------------------------------------------------------
# Block RAM inference hints.
#
# The corner-turn buffer is the single largest object in the design at 228
# BRAM36K tiles.  Vivado will infer it as block RAM from the coding style, but
# a synthesis pass that decides otherwise would blow the device up into LUTRAM
# and fail placement with a confusing message, so the intent is stated.
#-----------------------------------------------------------------------------
set_property RAM_STYLE BLOCK [get_cells -hier -quiet -filter {NAME =~ *corner_turn*/buf_*}]
set_property RAM_STYLE BLOCK [get_cells -hier -quiet -filter {NAME =~ *bitrev*/buf_*}]
set_property RAM_STYLE BLOCK [get_cells -hier -quiet -filter {NAME =~ *cfar*/line_*}]

# The window and twiddle tables are small ROMs; distributed RAM is cheaper for
# them than burning a BRAM tile each.
set_property ROM_STYLE DISTRIBUTED [get_cells -hier -quiet -filter {NAME =~ *lut_sin*}]
set_property ROM_STYLE DISTRIBUTED [get_cells -hier -quiet -filter {NAME =~ *lut_cos*}]

#-----------------------------------------------------------------------------
# Keep the DSP pipeline registers inside the DSP48E1 slices.  Without this the
# multiplier output register migrates into fabric and the multiply path becomes
# the critical path for no reason.
#-----------------------------------------------------------------------------
set_property USE_DSP yes [get_cells -hier -quiet -filter {NAME =~ *dechirp*}]

#-----------------------------------------------------------------------------
# Out-of-context I/O timing, used only for the standalone resource and timing
# check.  Inside the b200 design these ports are internal and the constraint is
# harmless.
#-----------------------------------------------------------------------------
set_input_delay  -clock radio_clk 2.0 [get_ports -quiet {rx0_i[*] rx0_q[*] rx1_i[*] rx1_q[*]}]
set_output_delay -clock radio_clk 2.0 [get_ports -quiet {tx0_i[*] tx0_q[*] tx1_i[*] tx1_q[*]}]
