#!/usr/bin/env ruby
# A very naive bridge support metadata verifier, based on HeaderDoc.
# Created by Laurent Sansonetti, 2006/12/09
# Copyright (c) 2006 Apple Computer Inc.

require 'rexml/document'
require 'tmpdir'
require 'fileutils'

def warn(*msg)
  STDERR.puts msg
end

def die(*msg)
  warn(msg)
  exit 1
end

$failures = 0
def failure(msg)
  warn(msg)
  $failures += 1
end

# Verifying if headerdoc is available.
headerdoc = `which headerdoc2html`
die "Can't locate the headerdoc2html program." unless $?.success?
headerdoc.strip!

# Checking args.
die "Usage: #{__FILE__} <metadata-file> <header-path>" if ARGV.length < 2
md_path, header_paths = ARGV.first, ARGV[1..-1]
die "Given metadata path '#{md_path}' doesn't exist." unless File.exist?(md_path)
header_paths.each { |p| die "Given header path '#{p}' doesn't exist." unless File.exist?(p) }

# Creating the temporary headerdoc output directory.
tmp_out_dir = File.join(Dir.tmpdir, 'headerdoc-out')
if File.exist?(tmp_out_dir)
  FileUtils.rm_rf(tmp_out_dir)
end
FileUtils.mkdir_p(tmp_out_dir)

# Running headerdoc.
unless system("#{headerdoc} -X -o #{tmp_out_dir} #{header_paths.join(' ')} &> /dev/null")
  die "Error when running headerdoc: #{$?}"
end

# Assembling the documents.
mdoc = REXML::Document.new(File.read(md_path))
hddocs = Dir.glob(File.join(tmp_out_dir, '**', '*.xml')).map { |p| REXML::Document.new(File.read(p)) }

# Verify!
hddocs.each do |hddoc|

  # Functions
  hddoc.get_elements('header/functions/function').each do |hdfunc|
    name = hdfunc.get_elements('name').first.text.strip
    name.sub!(/\(.+$/, '') # Sometimes the args are mentionned
    argc = hdfunc.get_elements('parameterlist/parameter').size
    if argc == 0
      argc = hdfunc.get_elements('parsedparameterlist/parsedparameter').size
    end
    res = mdoc.get_elements("signatures/function[@name='#{name}']")
    if res.empty?
      failure "Function '#{name}' not present"
    else
      margc = res.first.get_elements('function_arg').size
      if argc != margc
        failure "Function '#{name}' doesn't have #{argc} arg(s) (but #{margc})"
      end
    end 
  end

  # Constants
  hddoc.get_elements('header/constants/constant').each do |hdconst|
    name = hdconst.get_elements('name').first.text.strip
    res = mdoc.get_elements("signatures/constant[@name='#{name}']")
    failure "Constant '#{name}' not present" if res.empty?
  end

  # Enums
  hddoc.get_elements('header/enums/enum').each do |hdenumg|
    hdenumg.get_elements('parsedparameterlist/parsedparameter').each do |hdenum| 
      name = hdenum.get_elements('type').first.text.strip
      name.sub!(/\s*\=.*$/, '') # Sometimes the enum type ends with a '='
      res = mdoc.get_elements("signatures/enum[@name='#{name}']")
      failure "Enum '#{name}' not present" if res.empty?
    end 
  end

  # Informal protocols
  hddoc.get_elements('header/classes/category').each do |hdcategory|
    name = hdcategory.get_elements('name').first.text.strip
    md = /^NSObject\s*\(\s*([^)]+\s*)\)$/.match(name)
    next if md.nil?
    res = mdoc.get_elements("signatures/informal_protocol[@name='#{md[1]}']")
    if res.empty?
      failure "Informal protocol '#{md[1]}' not present" if res.empty?
    else
      # TODO: verify the method selectors
    end
  end
end

puts "Verification ended with #{$failures} failure(s)."
exit $failures > 0
