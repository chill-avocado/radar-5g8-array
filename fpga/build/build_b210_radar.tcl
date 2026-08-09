#=============================================================================
# build_b210_radar.tcl -- splice the radar core into the UHD b200 FPGA design
# and produce a loadable bitstream for the Kintex-7 B210 clone.
#
#   vivado -mode batch -source fpga/build/build_b210_radar.tcl \
#          -tclargs /path/to/uhd/fpga/usrp3/top/b200  [part]
#
# Vivado does not run on macOS, so this is for a Linux machine or a cloud
# instance.  Everything before it -- the RTL and its bit-exact verification --
# runs on the laptop under Verilator, so a build here should be a formality
# rather than a debugging session.
#
# ---------------------------------------------------------------------------
# WHAT THIS ACTUALLY DOES TO THE STOCK DESIGN
# ---------------------------------------------------------------------------
# The stock b200 design carries AD9361 sample streams between the radio front
# end and the CHDR packetiser.  The radar core is inserted in that path:
#
#   AD9361 RX --> [radar_top] --> CHDR out        instead of raw IQ
#   AD9361 TX <-- [radar_top]                     chirp generated in fabric
#
# and the settings bus that already reaches the radio is fanned out to the
# radar's register file.  Nothing else in the design is touched: USB, the
# control path, timekeeping and the AD9361 SPI interface are all stock, so UHD
# still enumerates the board as a B210 and every non-radar use of it still
# works.  That is deliberate -- a modified board that can no longer be used as
# an ordinary SDR would be a worse platform, not a better one.
#
# When ctrl_enable is low the core is transparent and passes raw IQ through,
# so the same bitstream is both the radar and an ordinary B210.
#=============================================================================

if {$argc < 1} {
    puts "usage: vivado -mode batch -source build_b210_radar.tcl -tclargs <uhd b200 top dir> \[part\]"
    exit 1
}
set b200_top [lindex $argv 0]
set part     "xc7k325tffg900-2"
if {$argc > 1} { set part [lindex $argv 1] }

set here [file dirname [file normalize [info script]]]
set rtl  [file normalize $here/../rtl]
set out  [file normalize $here/../../build_b210]
file mkdir $out

if {![file isdirectory $b200_top]} {
    puts "ERROR: $b200_top is not a directory."
    puts "Clone UHD and point at fpga/usrp3/top/b200:"
    puts "   git clone --depth 1 https://github.com/EttusResearch/uhd"
    exit 1
}

puts "== B210 radar bitstream =="
puts "   b200 source : $b200_top"
puts "   radar rtl   : $rtl"
puts "   part        : $part"
puts "   output      : $out"

create_project -force b210_radar $out/proj -part $part
set_property target_language Verilog [current_project]

#-----------------------------------------------------------------------------
# Stock b200 sources, then ours.  Ours are added last so that radar_top and its
# children take precedence if a name ever collides; every module we add is
# prefixed radar_ to make that collision unlikely in the first place.
#-----------------------------------------------------------------------------
foreach d [list \
        $b200_top \
        $b200_top/../../lib/control \
        $b200_top/../../lib/dsp \
        $b200_top/../../lib/fifo \
        $b200_top/../../lib/packet_proc \
        $b200_top/../../lib/radio \
        $b200_top/../../lib/timing \
        $b200_top/../../lib/vita ] {
    if {[file isdirectory $d]} {
        set f [glob -nocomplain $d/*.v $d/*.sv $d/*.vh]
        if {[llength $f]} { add_files -norecurse $f }
    }
}

add_files -norecurse [glob $rtl/*.v]
add_files -fileset constrs_1 -norecurse $here/radar.xdc
foreach x [glob -nocomplain $b200_top/*.xdc] {
    add_files -fileset constrs_1 -norecurse $x
}

set_property include_dirs [list $rtl $b200_top] [current_fileset]
set_property top b200 [current_fileset]
update_compile_order -fileset sources_1

#-----------------------------------------------------------------------------
# The insertion itself is a source edit, not something a TCL script should do
# behind your back.  integrate_b200.sh makes it, reversibly, and prints the
# diff.  Refuse to build a bitstream that has not had it applied, rather than
# producing a stock image with a misleading filename.
#-----------------------------------------------------------------------------
set marker_found 0
foreach f [glob -nocomplain $b200_top/b200_core.v $b200_top/b200.v] {
    set fh [open $f r]
    if {[string match "*RADAR_CORE_INSERTED*" [read $fh]]} { set marker_found 1 }
    close $fh
}
if {!$marker_found} {
    puts "ERROR: the b200 sources have not been patched for the radar core."
    puts "Run:  python3 fpga/build/integrate_b200.py $b200_top"
    puts "It is reversible and prints exactly what it changes."
    exit 1
}

#-----------------------------------------------------------------------------
launch_runs synth_1 -jobs 8
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    puts "SYNTHESIS FAILED"; exit 1
}

launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    puts "IMPLEMENTATION FAILED"; exit 1
}

open_run impl_1
report_utilization    -file $out/utilization.rpt
report_timing_summary -file $out/timing.rpt

set wns [get_property STATS.WNS [get_runs impl_1]]
puts [format "  worst negative slack %+.3f ns" $wns]
if {$wns < 0} { puts "TIMING FAILED -- see $out/timing.rpt"; exit 1 }

#-----------------------------------------------------------------------------
# UHD loads a raw .bin, not a .bit.  Bitstream compression is on because the
# vendor image uses it (5.2 MB against 11.4 MB uncompressed) and the board's
# configuration flash is sized for that.
#-----------------------------------------------------------------------------
set_property BITSTREAM.GENERAL.COMPRESS      TRUE   [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH   4      [current_design]
write_bitstream -force -bin_file $out/usrp_b210_radar

file copy -force $out/usrp_b210_radar.bin $out/usrp_b210_fpga.bin
puts "-------------------------------------------------------------"
puts "  bitstream: $out/usrp_b210_fpga.bin"
puts "  load it with:  uhd_image_loader --args=\"type=b200,fpga=$out/usrp_b210_fpga.bin\""
puts "  KEEP A COPY OF THE VENDOR IMAGE. It is the way back."
puts "-------------------------------------------------------------"
exit 0
