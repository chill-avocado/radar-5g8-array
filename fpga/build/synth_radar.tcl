#=============================================================================
# synth_radar.tcl -- out-of-context synthesis of the radar core alone
#
# Answers two questions before anyone spends an hour on a full build:
#   does it fit, and does it close timing at 61.44 MHz?
# It does not produce a bitstream.  For that, use build_b210_radar.tcl, which
# splices this core into the UHD b200 design.
#
#   vivado -mode batch -source fpga/build/synth_radar.tcl
#   vivado -mode batch -source fpga/build/synth_radar.tcl -tclargs xc7k325tffg676-2
#
# Vivado does not run on macOS.  This is meant for a Linux box or a cloud
# instance; everything up to this point -- the RTL and its verification -- runs
# on the laptop under Verilator.
#=============================================================================

set part "xc7k325tffg900-2"
if {$argc > 0} { set part [lindex $argv 0] }

# The exact package of the Kintex-7 on the OpenSourceSDRLab board is not
# recorded anywhere we can read; the die is confirmed as XC7K325T from the
# vendor bitstream's IDCODE 0x03651093.  Package and speed grade only affect
# I/O placement and the last few percent of timing, neither of which changes
# whether this core fits.  Override with -tclargs if you know the package.
puts "== radar core out-of-context synthesis, part $part =="

set here   [file dirname [file normalize [info script]]]
set rtl    [file normalize $here/../rtl]
set outdir [file normalize $here/../../build_ooc]
file mkdir $outdir

create_project -in_memory -part $part

read_verilog -sv [glob $rtl/*.v]
set_property include_dirs [list $rtl] [current_fileset]
read_xdc $here/radar.xdc

synth_design -top radar_top -part $part -mode out_of_context \
             -flatten_hierarchy rebuilt -directive AreaOptimized_high

opt_design
report_utilization        -file $outdir/utilization.rpt
report_utilization -hierarchical -file $outdir/utilization_hier.rpt
report_timing_summary     -file $outdir/timing.rpt
report_ram_utilization    -file $outdir/ram.rpt -quiet
write_checkpoint -force   $outdir/radar_core_synth.dcp

#-----------------------------------------------------------------------------
# Fail loudly rather than leaving a passing-looking log behind.
#-----------------------------------------------------------------------------
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
set luts [get_property USED [get_report_property -quiet]]

set slice_luts [expr {[llength [get_cells -hier -filter {PRIMITIVE_GROUP == LUT}]}]
set bram36     [llength [get_cells -hier -filter {REF_NAME =~ RAMB36*}]]
set bram18     [llength [get_cells -hier -filter {REF_NAME =~ RAMB18*}]]
set dsp        [llength [get_cells -hier -filter {REF_NAME =~ DSP48*}]]

puts "-------------------------------------------------------------"
puts [format "  LUTs        %8d  of 203800   (%.1f%%)" $slice_luts [expr {100.0*$slice_luts/203800}]]
puts [format "  BRAM36      %8d  of 445      (%.1f%%)" $bram36 [expr {100.0*$bram36/445}]]
puts [format "  BRAM18      %8d" $bram18]
puts [format "  DSP48E1     %8d  of 840      (%.1f%%)" $dsp [expr {100.0*$dsp/840}]]
puts [format "  worst setup slack  %+.3f ns  (period 16.276 ns)" $wns]
puts "-------------------------------------------------------------"

if {$wns < 0} {
    puts "TIMING FAILED -- see $outdir/timing.rpt"
    exit 1
}
if {$bram36 > 400} {
    puts "BRAM OVER BUDGET -- the radio core still needs its share"
    exit 1
}
puts "PASS: the radar core fits and closes timing at 61.44 MHz"
exit 0
