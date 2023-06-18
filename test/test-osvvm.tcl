# Clones OsvvmLibraries, runs all tests. Used by the test-osvvm.yml workflow.
# To run manually:
#   tclsh test-osvvm.tcl
# Note: tcllib is required for OSVVM

if {[info exists env(OSVVM_DIR)]} {
    set OsvvmDir $env(OSVVM_DIR)
} else {
    # known good snapshot
    set OsvvmLibraries_tag "2023.05"
    set OsvvmDir [file join $::env(HOME) .cache nvc "OsvvmLibraries-${OsvvmLibraries_tag}"]
}

source $OsvvmDir/Scripts/StartNVC.tcl

if {[info exists env(OSVVM_MUST_BUILD)]} {
    build $OsvvmDir/OsvvmLibraries
}

build $OsvvmDir/RunAllTests
build $OsvvmDir/RunAllTestsVti
