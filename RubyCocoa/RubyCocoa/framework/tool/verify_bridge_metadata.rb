# Copyright (c) 2007, The RubyCocoa Project.
# All Rights Reserved.
#
# RubyCocoa is free software, covered under either the Ruby's license or the 
# LGPL. See the COPYRIGHT file for more information.

# A BridgeSupport files verifier, based on the generated Ruby files from 
# gen_bridge_doc.
#
# To run it from the source root:
#    ruby framework/tool/verify_bridge_metadata.rb framework/bridge-support 
#       framework/bridge-doc
#
# (Make sure to have run install.rb config and doc tasks before.)

begin require 'rubygems'; rescue LoadError; end
require 'xml/libxml'

def die(s)
  $stderr.puts s
  exit 1
end

def log(s)
  $stderr.puts s if $VERBOSE
end

def lputs(p, s)
  puts p.ljust(50) + s
end

# Check arguments.
if ARGV.size != 2
  die "Usage: #{__FILE__} <bridge-support-dir> <bridge-doc-dir>"
end
bs_dir, doc_dir = ARGV
$doc_ruby_dir = doc_ruby_dir = File.join(doc_dir, 'ruby')

# Setup some autoload magic first.
module OSX
  def self.const_missing(sym)
    return Object if sym == :ObjcID
    path = "#{$doc_ruby_dir}/#{sym}.rb"
    if File.exist?(path)
      require(path)
      if const_defined?(sym)
        const_get(sym)
      end
    end
  end
end

# Load bridge support and Ruby files.
class BSDocs
  def initialize(bs_dir)
    @bs_docs = Dir.glob(bs_dir + '/*.bridgesupport').map { |f| XML::Document.file(f) } 
  end
  def any?(xpath)
    xpath = '/signatures/' + xpath
    @bs_docs.each do |doc|
      set = doc.find(xpath)
      return set[0] unless set.empty? 
    end
    return nil
  end
end
bs_docs = BSDocs.new(bs_dir) 
Dir.glob(doc_ruby_dir + '/*.rb').map { |x| require(x) }

class BSTest
  attr_accessor :name, :failures, :count
  def self.go(name)
    test = self.new 
    test.name = name
    test.failures = []
    test.count = 0
    @tests ||= []
    @tests << test
    yield(test)
  end
  def self.all_tests
    @tests
  end
  def success(msg)
    log(msg)
    @count += 1
  end
  def failure(msg)
    log(msg)
    @failures << msg
    @count += 1
  end
end

log 'Verifying functions...'
BSTest.go('Functions') do |test|
  OSX.methods(false).each do |func_name|
    if elem = bs_docs.any?("function[@name=\"#{func_name}\"]")
      argc = elem.find('arg').length
      e_argc = OSX.method(func_name).arity
      if e_argc < 0 and elem['variadic'] == 'true'
        e_argc = e_argc.abs - 1
      end     
      if e_argc == argc
        test.success func_name
      else
        test.failure "#{func_name} has wrong arity (expected #{e_argc}, got #{argc})"
      end
    else
      test.failure "#{func_name} is missing"
    end  
  end
end

log 'Verifying constants...'
BSTest.go('Constants') do |test|
  OSX.constants.each do |const_name|
    o = OSX.const_get(const_name)
    next unless o.nil? # Constants are nil
    low_const_name = const_name.sub(/^[A-Z]/) { |s| s.downcase }
    if bs_docs.any?("constant[@name=\"#{const_name}\" or @name=\"#{low_const_name}\"]") \
       or bs_docs.any?("enum[@name=\"#{const_name}\" or @name=\"#{low_const_name}\"]")
      
      test.success const_name
    else
      test.failure "#{const_name} is missing"
    end 
  end
end

# Print test results
BSTest.all_tests.each do |test|
  puts "#{test.name}: #{test.count} test(s), #{test.failures.length} failure(s)"
  test.failures.each { |s| puts "-> " + s }
end
