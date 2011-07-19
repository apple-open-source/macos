package require starkit

# old tclkits have broken "sourced" symlink detection (in their "starkit" pkg)
# let SDX work in older ones but don't allow sourcing, which is a new feature
if {[starkit::startup] eq "sourced" &&
    [package vcompare [info patchlevel] 8.4.9] >= 0} return

package require app-sdx
