#! /usr/bin/ruby

# This script takes a text file describing function prologues and
# emits a C file with the necessary C/assembly functions plus a
# dejagnu ".exp" file to do several tests on each of the patterns.


$filename_base = "gen-x86-prologues"

####### add_prototypes

def add_prototype (str, pat)
  str + "void #{pat} (void);\n" + "void func_under_#{pat} (void);\n"
end

####### add_main_call

def add_main_call (str, pat)
  str + "  #{pat} ();\n"
end

####### add_function

def add_function (str, pat, prologue, epilogue)
  t = <<HERE
asm ("  .text\\n"
     "  .align 8\\n"
     "_#{pat}:\\n"
HERE
  middle = <<HERE
     "   call _func_under_#{pat}\\n"
HERE
  
  p = String.new
  prologue.each_line do |l|
    p = p + l.chomp.gsub(/^/, '     "').gsub(/$/, "\\n\"\n")
  end
  e = String.new
  epilogue.each_line do |l|
    e = e + l.chomp.gsub(/^/, '     "').gsub(/$/, "\\n\"\n")
  end

  func_under = <<HERE

void func_under_#{pat} (void)
{
   puts ("I am the function under #{pat}");
}
HERE

  str + t + p + middle + e + ");\n" + func_under + "\n"
end

####### add_expect_breakpoint

def add_expect_breakpoint (str, pat)
  t = <<HERE
gdb_test "b func_under_#{pat}" "Breakpoint $decimal at 0x.*" "set breakpoint on func_under_#{pat}"
HERE
  str + t
end

####### add_expect_body

def add_expect_body (str, pat)
  t = <<HERE
gdb_test "continue" "Continuing.*Breakpoint $decimal, func_under_#{pat} .*#{$filename_base}.*" "continue to func_under_#{pat}"
gdb_test "bt" ".*#0  func_under_#{pat} \\\\(\\\\) at .*pro.*#1  $hex in #{pat} \\\\(\\\\).*#2  $hex in main \\\\(argc=1.*" "backtrace in #{pat}"
gdb_test "fin" ".*Run till exit from #0  func_under_#{pat}.*" "finish from func_under_#{pat} to #{pat}"
gdb_test "bt" ".*#0  $hex in #{pat} \\\\(\\\\).*#1  $hex in main \\\\(argc=1.*" "backtrace in #{pat}"
gdb_test "fin" ".*Run till exit from #0  $hex in #{pat}.*" "finish from #{pat} to main"
gdb_test "bt" ".*#0  main \\\\(argc=1.*" "backtrace in main (from #{pat})"
gdb_test "maint i386-prologue-parser #{pat}" ".*Found push %ebp.*Found mov %esp.*" "analyze #{pat} prologue directly"
HERE
  str + t
end



expect_head = <<HERE
if $tracelevel then {
	strace $tracelevel
}

set prms_id 0
set bug_id 0

set testfile "#{$filename_base}"
set srcfile ${testfile}.c
set binfile ${objdir}/${subdir}/${testfile}

global hex decimal

if ![istarget "i\\[3-6\\]86-apple-darwin*"] {
    verbose "Skipping x86 prologue tests."
    return
}

if [target_info exists darwin64] {
   verbose "This test file not yet adapted for x86-64, skipping."
   return
}

if  { [gdb_compile "${srcdir}/${subdir}/$srcfile" "${binfile}" executable {debug}] != "" } {
     gdb_suppress_entire_file "Testcase compile failed, so all tests in this fil
e will automatically fail."
}


# Start with a fresh gdb

gdb_exit
gdb_start
gdb_reinitialize_dir $srcdir/$subdir
gdb_load ${binfile}

if ![runto_main] then { 
  fail "#{$filename_base} tests suppressed"
  return -1
}
HERE






#####
##### main routine
#####



cfile_prototypes = String.new
cfile_main = "static int word_of_writable_memory;\nmain (int argc, char **argv, char **envp)\n{\n"
cfile_funcs = String.new
expect_breakpoints = String.new
expect_body = String.new
ptmp = String.new

current_prologue = nil
current_epilogue = nil
pattern_count = 1

File.open("#{$filename_base}-patterns.txt").each_line do |line|
  line.chomp

# If we're seeing # PROLOGUE we're either seeing our first pattern or
# we've just completed reading a pattern and are starting a new one.

  if line =~ /^# PROLOGUE/
    if !current_prologue.nil? && !current_epilogue.nil?
      ptmp = "pattern_#{pattern_count}"
      cfile_prototypes = add_prototype(cfile_prototypes, ptmp)
      cfile_main = add_main_call(cfile_main, ptmp)
      cfile_funcs = add_function(cfile_funcs, ptmp, \
                                 current_prologue, current_epilogue)
      expect_breakpoints = add_expect_breakpoint(expect_breakpoints, ptmp)
      expect_body = add_expect_body(expect_body, ptmp)
      pattern_count = pattern_count + 1
    end
    current_prologue = String.new
    current_epilogue = nil
    next
  end
  if line =~ /^# EPILOGUE/
    current_epilogue = String.new
    next
  end
  if line =~ /^\s*#/ || line =~ /^\s*$/
    next
  end
  if current_prologue.nil? && current_epilogue.nil? 
    next
  end

# We've got some text to add to the current pattern; either add it to
# the current_prologue or current_epilogue.

  if current_epilogue.nil?
    current_prologue = current_prologue + line
  else
    current_epilogue = current_epilogue + line
  end
end

# Add the last pattern we saw

ptmp = "pattern_#{pattern_count}"
cfile_prototypes = add_prototype(cfile_prototypes, ptmp)
cfile_main = add_main_call(cfile_main, ptmp)
cfile_funcs = add_function(cfile_funcs, ptmp, current_prologue, current_epilogue)
expect_breakpoints = add_expect_breakpoint(expect_breakpoints, ptmp)
expect_body = add_expect_body(expect_body, ptmp)

if File.file?("#{$filename_base}.exp")
  File.unlink("#{$filename_base}.exp")
end
if File.file?("#{$filename_base}.c")
  File.unlink("#{$filename_base}.c")
end

# Generate the .c and .exp files.

File.open("#{$filename_base}.exp", "w") do |out|
  out.puts "#### NOTE NOTE NOTE NOTE  THIS IS A GENERATED FILE!!"
  out.puts "#### Any edits will be deleted!"
  out.puts "#### Instead, change #{$filename_base}.rb and re-generate."
  out.puts "####"
  out.puts "\n\n"
  out.puts expect_head
  out.puts expect_breakpoints
  out.puts expect_body
  out.puts "\ngdb_exit\nreturn 0\n"
end
File.open("#{$filename_base}.c", "w") do |out|
  out.puts "// NOTE NOTE NOTE NOTE  THIS IS A GENERATED FILE!!"
  out.puts "// Any edits will be deleted!"
  out.puts "// Instead, change #{$filename_base}.rb and re-generate."
  out.puts "//"
  out.puts "\n\n"
  out.puts cfile_prototypes
  out.puts "\n"
  out.puts cfile_main
  out.puts "}\n\n"
  out.puts cfile_funcs
end
