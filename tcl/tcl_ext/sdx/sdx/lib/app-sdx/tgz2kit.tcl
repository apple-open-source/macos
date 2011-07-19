# convert a tar/gz file to a VFS-based Starkit - Nov 2002, jcw@equi4.com

# option -notop flag drops top-level directory name from the paths
  set first 0
  if {[lindex $argv 0] eq "-notop"} {
    set first 1
    set argv [lrange $argv 1 end]
  }
# input file must be specified on the command line
  set infile [lindex $argv 0]
  if {![file isfile $infile]} {
    puts stderr "usage: $argv0 ?-notop? targzfile"
    exit 1
  }
# output file is in local dir, same as infile, but with a .kit extension
  set outfile [file tail $infile]
  if {[string match *.tar.gz $outfile]} {
    set outfile [file root $outfile]
  }
  set outfile [file root $outfile].kit

#  if {[file exists $outfile]} {
#    puts stderr "$outfile: already exists, bailing out"
#    exit 1
#  }

# open the input through gzip to decompress
  set ifd [open "| gzip -d < $infile"]
  fconfigure $ifd -translation binary
# a messy way to write a starkit header with some special chars in it
  set ofd [open $outfile w]
  fconfigure $ofd -translation binary
  puts $ofd [format {#!/bin/sh
    # %c
    exec tclsh "$0" ${1+"$@"}
    package require starkit
    starkit::header mk4 -readonly
    ######
    %c} 0x5C 0x1A]
  close $ofd
# ok, work with a mounted VFS
  vfs::mk4::Mount $outfile $outfile
  set opwd [pwd]
  cd $outfile
# now go through each of the unpacked tar file entries and copy them
  set nfiles 0
  while {[set header [read $ifd 512]] != ""} {
    binary scan $header A100A8A8A8A12A12A8A1A100A6A6A32A32A8A8A155 \
			name mode uid gid size mtime cksum typeflag \
			linkname ustar_p ustar_vsn uname gname devmaj \
			devmin prefix
    if {$name eq ""} continue
    set name [eval file join . [lrange [file split $name] $first end]]
    if {$name eq "."} continue

    if {$size != ""} {
      set data [read $ifd $size]
      set n [expr {(($size+511)/512)*512 - $size}]
      if {$n > 0} { read $ifd $n }
    }

    if {$typeflag eq "5"} {
      file mkdir $name
    } elseif {($typeflag eq "0" || $typeflag eq "\0") && $size ne ""} {
      file mkdir [file dirname $name]
      set ofd [open $name w]
      fconfigure $ofd -translation binary
      puts -nonewline $ofd $data
      close $ofd
      file mtime $name [expr 0$mtime+0]
      incr nfiles
    }
  }
# done, flush all changes
  close $ifd
  cd $opwd
  vfs::unmount $outfile
  file mtime $outfile [file mtime $infile]
# report some statistics about what was done
  puts "$nfiles files, [file size $infile]b in, [file size $outfile]b out"
