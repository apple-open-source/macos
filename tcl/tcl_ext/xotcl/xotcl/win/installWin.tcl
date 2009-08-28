# $Id: installWin.tcl,v 1.1 2004/05/23 22:50:39 neumann Exp $
if {$::argc > 0} {
  set DESTINATION [lindex $argv 0]
} else {
  # if now argument is given -> set to default
  set DESTINATION "/Progra~1/Tcl"
}

puts "Installing to Directory: $DESTINATION"

#rename file tcl_file
#proc file args {
#  puts stderr "...$args"
#  eval ::tcl_file $args
#}

#
# find out the XOTcl version
#
cd ..
set xotclDir .
set buildDir $xotclDir/win/Release
cd $buildDir
set xotcllib [lindex [glob -nocomplain libxotcl*.dll] 0]
cd ../..
regexp {^libxotcl([0-9]\.[0-9]).dll$} $xotcllib _ xotclVersion
puts "XOTcl Version is   $xotclVersion"
set MAKEXOTCL $xotclDir/library/lib/make.xotcl

#
# copy binaries
#
file mkdir $DESTINATION/bin
set bin "xotclsh.exe xowish.exe"
foreach b $bin {
  if {[file exists $buildDir/$b]} {
    puts "Copying $b"
    file copy -force $buildDir/$b $DESTINATION/bin
  } elseif {[file exists  $xotclDir/$b]} {
    puts "Copying $b"
    file copy -force $xotclDir/$b $DESTINATION/bin
  }
}

#
# copy libraries
#
file mkdir $DESTINATION/include
set includes "xotcl.h xotclInt.h xotclDecls.h xotclIntDecls.h"
foreach i $includes {
    puts "Copying $i"
    file copy -force $xotclDir/generic/$i $DESTINATION/include
}

#
# create the XOTcl library directory
#
puts "Creating XOTcl Lib Dir"
file mkdir $DESTINATION/lib
file mkdir $DESTINATION/lib/xotcl$xotclVersion

#
# copy the dll
#
set b libxotcl${xotclVersion}.dll
if {[file exists $buildDir/$b]} {
    puts "Copying $b"
    file copy -force $buildDir/$b $DESTINATION/lib
} elseif {[file exists $xotclDir/$b]} {
    puts "Copying $b"
    file copy -force $xotclDir/$b $DESTINATION/lib
}

#
# create a pkgindex file for xotcl in the library directoy
#
#file mkdir $DESTINATION/lib/xotcl$xotclVersion/libxotcl
puts "Creating pkgindex files for xotcl in the library directory"
#set pkgIndex "package ifneeded XOTcl $xotclVersion \[list load \[file join \$dir ../../../bin \"libxotcl${xotclVersion}.dll\"\] XOTcl\]"
#set F [open $DESTINATION/lib/xotcl$xotclVersion/libxotcl/pkgIndex.tcl w]
#puts $F $pkgIndex
#puts $F "\n"
#close $F
## and one in the xotcl-<VERSION> directory
set pkgIndex "package ifneeded XOTcl $xotclVersion \[list load \[file join \$dir ../../ \"libxotcl${xotclVersion}.dll\"\] XOTcl\]"
file mkdir $DESTINATION/lib/xotcl$xotclVersion/xotcl
set F [open $DESTINATION/lib/xotcl$xotclVersion/xotcl/pkgIndex.tcl w]
puts $F $pkgIndex
puts $F "\n"
close $F

proc copyDir {dir dest} {
  puts "Copying directory: $dir"
  set files [glob -nocomplain $dir/*]
  foreach f $files {
    doDir $f $dest
  }
}

proc doDir {f dest} {
  if {[file isdirectory $f]} {
    file mkdir $dest/$f
    copyDir $f $dest
  } else {
    file copy -force $f $dest/$f
 }
}

cd library

foreach f [glob -nocomplain *] {
  doDir $f $DESTINATION/lib/xotcl$xotclVersion
}

cd ..
set pwd [pwd]

puts "Indexing the packages"
set shell [info nameofexecutable]
#puts "SHELL=$shell"
cd $DESTINATION/lib/xotcl$xotclVersion
if {[catch {exec $shell $pwd/$MAKEXOTCL -target $DESTINATION/lib/xotcl$xotclVersion -all} errMsg]} {
  puts "  ... resulting message: $errMsg\n"
} else {
  puts " ... Package Indexing SUCCEEDED\n"
}

puts stderr "***************************************************"
puts stderr "INSTALLATION COMPLETE."
puts stderr "\nIn order to use XOTcl set PATH to "
puts stderr "  '$DESTINATION/bin' and"
puts stderr "set TCLLIBPATH to "
puts stderr "  '$DESTINATION/lib/xotcl$xotclVersion'"
puts stderr "***************************************************"


