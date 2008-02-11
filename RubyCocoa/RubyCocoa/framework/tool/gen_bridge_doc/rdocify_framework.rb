#!/usr/bin/env ruby
# Copyright (c) 2006-2007, The RubyCocoa Project.
# All Rights Reserved.
#
# RubyCocoa is free software, covered under either the Ruby's license or the 
# LGPL. See the COPYRIGHT file for more information.

script_dir = File.dirname(File.expand_path(__FILE__))
require "#{script_dir}/lib/cocoa_ref"

def command( str )
  $stderr.puts str
  system str or raise RuntimeError, "'system #{str}' failed"
end

def ruby( str )
  command "#{Config::CONFIG["bindir"]}/ruby #{str}"
end

def rdoc( str )
  command "#{Config::CONFIG["bindir"]}/rdoc #{str}"
end

def get_reference_files(framework_path)
  reference_files = []
  
  # Get the Class reference files
  classes_dir = File.join(framework_path, 'Classes/')
  Dir.entries(classes_dir).each do |f|
    class_path = File.join(classes_dir, f)
    if File.directory?(class_path) and not f == '.' and not f == '..'
      ref_dir_path = File.join(class_path, 'Reference/')
      ref_dir_path = File.join(class_path, 'Introduction/') unless File.exists?(ref_dir_path)
      Dir.entries(ref_dir_path).each do |rf|
        if File.extname(rf) == '.html'
          ref_path = File.join(ref_dir_path, rf)
          reference_files.push({:name => File.basename(class_path), :path => ref_path})
        end
      end
    end
  end
  
  # Get the Protocol reference files
  protocols_dir = File.join(framework_path, 'Protocols/')
  if File.exist?(protocols_dir)
    Dir.entries(protocols_dir).each do |f|
      protocol_path = File.join(protocols_dir, f)
      if File.directory?(protocol_path) and not f == '.' and not f == '..'
        ref_dir_path = File.join(protocol_path, 'Reference/')
        Dir.entries(ref_dir_path).each do |rf|
          if File.extname(rf) == '.html'
            ref_path = File.join(ref_dir_path, rf)
            reference_files.push({:name => File.basename(protocol_path), :path => ref_path})
          end
        end
      end
    end
  end
  
  misc_dir = File.join(framework_path, 'Miscellaneous/')
  if File.exist?(misc_dir)
    Dir.entries(misc_dir).each do |f|
      if f.include? 'Constants'
        # Get the constants reference file
        ref_dir_path = File.join(misc_dir, f, 'Reference/')
        Dir.entries(ref_dir_path).each do |rf|
          if File.extname(rf) == '.html'
            ref_path = File.join(ref_dir_path, rf)
            reference_files.push({:name => "#{File.basename(framework_path)}Constants", :path => ref_path})
          end
        end
      end
    end
    Dir.entries(misc_dir).each do |f|
      if f.include? 'Functions'
        # Get the functions reference file
        ref_dir_path = File.join(misc_dir, f, 'Reference/')
        Dir.entries(ref_dir_path).each do |rf|
          if File.extname(rf) == '.html'
            ref_path = File.join(ref_dir_path, rf)
            reference_files.push({:name => "#{File.basename(framework_path)}Functions", :path => ref_path})
          end
        end
      end
    end
    Dir.entries(misc_dir).each do |f|
      if f.include? 'DataTypes'
        # Get the data types reference file
        ref_dir_path = File.join(misc_dir, f, 'Reference/')
        Dir.entries(ref_dir_path).each do |rf|
          if File.extname(rf) == '.html'
            ref_path = File.join(ref_dir_path, rf)
            reference_files.push({:name => "#{File.basename(framework_path)}DataTypes", :path => ref_path})
          end
        end
      end
    end
  end
  
  return reference_files
end

$COCOA_REF_DEBUG = false
output_files_with_errors = false
framework_path = ''
output_dir = ''

unless ARGV.empty?
  ARGV.each do |arg|
    case arg
    when '-f'
      output_files_with_errors = true
    when '-v'
      $COCOA_REF_DEBUG = true
    else
      if framework_path.empty?
        framework_path = arg
      else
        output_dir = arg
      end
    end
  end
else
  puts "Usage:"
  puts "  #{__FILE__} [options] path/to/the/framework <output dir>"
  puts ""
  puts "Options:"
  puts "  -v Verbose output."
  puts "  -f Force the output files to be written even if there were errors during parsing."
  puts ""
  puts "Example:"
  puts "  #{__FILE__} /Developer/ADC Reference Library/documentation/Cocoa/Reference/ApplicationKit/ output/"
  puts "  #{__FILE__} -v /Developer/ADC Reference Library/documentation/Cocoa/Reference/ApplicationKit/"
  puts "  #{__FILE__} -v -f /Developer/ADC Reference Library/documentation/Cocoa/Reference/ApplicationKit/"
  puts ""
  
  exit
end

framework_name = File.basename(framework_path)
# This is a workaround for pdfkit which is in the quartz dir,
# if it looks like that there are more frameworks that need
# special treatment than this should be done with a less hacky way.
framework_name = 'PDFKit' if framework_name == 'QuartzFramework'

puts ""
puts "Working on: #{framework_name}"
puts ""

reference_files = get_reference_files(framework_path)

# This is a workaround for pdfkit which is in the quartz dir,
# if it looks like that there are more frameworks that need
# special treatment than this should be done with a less hacky way.
if File.basename(framework_path) == 'QuartzFramework'
  reference_files.delete_if {|r| ['QCView_Class', 'QCRenderer_Class'].include?(r[:name]) }
end

unless output_dir.empty?
  output_dir = File.expand_path(output_dir)
else
  output_dir = File.join(script_dir, 'output/')
end

if not File.exists?(output_dir)
  command "mkdir -p #{output_dir}"
end

success = 0
skipped = 0
reference_files.each do |ref|
  puts "Processing reference file: #{ref[:name]}"
  cocoa_ref_parser = CocoaRef::Parser.new(ref[:path], framework_name)
  if cocoa_ref_parser.empty?
    skipped = skipped.next
    puts 'Skipping because there was no info found of our interest in the reference file...'
  elsif not cocoa_ref_parser.errors? or output_files_with_errors
    success = success.next
    puts "      Writing output file: #{cocoa_ref_parser.class_def.output_filename}"
    cocoa_ref_parser.to_rb_file(output_dir)
  else
    skipped = skipped.next
    puts 'Skipping because there were errors while parsing...'
  end
end

puts ''
puts "Stats for: #{framework_name}"
puts "  Written: #{success} files"
puts "  Skipped: #{skipped} files"
puts ''
