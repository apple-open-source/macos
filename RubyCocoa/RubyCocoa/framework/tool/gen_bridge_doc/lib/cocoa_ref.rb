# Copyright (c) 2006-2007, The RubyCocoa Project.
# All Rights Reserved.
#
# RubyCocoa is free software, covered under either the Ruby's license or the 
# LGPL. See the COPYRIGHT file for more information.

begin
  require 'hpricot'
rescue LoadError
  require 'rubygems'
  require 'hpricot'
end
require 'osx/cocoa'

lib_path = File.join(File.dirname(File.expand_path(__FILE__)), 'lib/')
require "#{lib_path}/extras"
require "#{lib_path}/class_additions"
require "#{lib_path}/clean_up"
require "#{lib_path}/hpricot_proxy"
require "#{lib_path}/class_def"
require "#{lib_path}/method_def"
require "#{lib_path}/constant_def"
require "#{lib_path}/notification_def"
require "#{lib_path}/function_def"
require "#{lib_path}/datatype_def"

module CocoaRef
  class Log
    def initialize
      @errors = []
    end
    
    def add(str)
      @errors.push(str)
      if $COCOA_REF_DEBUG
        puts str
      end
    end
    
    def errors?
      not @errors.empty?
    end
  end

  class Parser
    attr_reader :class_def
    
    def initialize(file, framework = '')
      @framework = framework.gsub(/Frame\w+/, '')
      unless framework == 'ApplicationKit' or framework == 'Foundation' or framework.empty?
        OSX.require_framework @framework
      end
      
      @log = CocoaRef::Log.new
      
      # LOAD THE HPRICOT PROXY AND ANY OVERRIDES IF NEEDED
      @hpricot = CocoaRef::HpricotProxy.new(file)
      html_parser_overrides_file = File.join(File.dirname(File.expand_path(__FILE__)), @framework, 'HTMLParserOverrides.rb')
      if File.exist?(html_parser_overrides_file)
        # If it exists, require it and extend the html parser to use the overrides
        require html_parser_overrides_file
        @hpricot.extend HTMLParserOverrides
      end
      
      # START PARSING
      @class_def = parse_reference(file)
      @class_def.framework = @framework
      
      # Check if there is a overrides file in the override_dir for the given class
      class_overrides_file = File.join(File.dirname(File.expand_path(__FILE__)), @framework, @class_def.output_filename)
      if File.exist?(class_overrides_file)
        # If it exists, require it and extend the methods to use the overrides
        require class_overrides_file
        @class_def.method_defs.each do |m|
          m.instance_eval "self.send :extend, #{@class_def.name}Overrides"
        end
        @class_def.delegate_method_defs.each do |dm|
          dm.instance_eval "self.send :extend, #{@class_def.name}Overrides"
        end
        @class_def.function_defs.each do |f|
          f.instance_eval "self.send :extend, #{@class_def.name}FunctionsOverrides"
        end
      end
    end
    
    def empty?
      @class_def.empty?
    end
    
    def errors?
      @log.errors? or @hpricot.log.errors? or @class_def.errors?
    end
    
    def parse_reference(file)
      class_def = ClassDef.new
      busy_with = ''
      constant_type = :normal
      index = 0
      @hpricot.elements.each do |element|
        if element.fits_the_description?('h1', 'Class Reference') or element.fits_the_description?('h1', 'Class Objective-C Reference')
          class_def.type = :class
          class_def.name = element.inner_html.strip_tags.split(' ').first
        elsif element.fits_the_description?('h1', 'Additions Reference')
          class_def.type = :additions
          class_def.name = element.inner_html.strip_tags.split(' ').first
        elsif element.fits_the_description?('h1', 'Deprecation Reference')
          # FIXME: this is a check for NSObjCTypeSerializationCallBack.
          # It's deprecated therefor it shows up here, but it's a protocol ref actually.
          # Maybe we need to skip deprecated refs anyway...
          name = element.inner_html.strip_tags.split(' ').first
          if name == 'NSObjCTypeSerializationCallBack'
            class_def.type = :protocols
            class_def.name = name
          else
            class_def.type = :class
            class_def.name = name
          end
        elsif element.fits_the_description?('h1', 'Functions Reference')
          class_def.type = :functions
          class_def.name = @framework
        elsif element.fits_the_description?('h1', 'Data Types Reference')
          class_def.type = :data_types
          class_def.name = @framework
        elsif element.fits_the_description?('h1', 'Constants Reference')
          class_def.type = :constants
          class_def.name = @framework
        elsif element.fits_the_description?('h1', 'Protocol') # FIXME: Had to remove the ref part because of a bug with appkit/protocols/NSIgnoreMisspelledWords
          class_def.type = :protocols
          class_def.name = element.inner_html.strip_tags.split(' ').first
        end

        if element.fits_the_description?('h2', 'Class Description')
          class_def.description = @hpricot.get_the_text(index + 1).first
        end

        if element.fits_the_description?('h2', 'Class Methods')
          busy_with = 'Class Methods'
        end
        if busy_with == 'Class Methods' and @hpricot.start_of_method_def?(index)
          class_def.method_defs.push @hpricot.get_method_def(index, :class_method)
        end
        
        if element.fits_the_description?('h2', 'Instance Methods')
          busy_with = 'Instance Methods'
        end
        if busy_with == 'Instance Methods' and @hpricot.start_of_method_def?(index)
          class_def.method_defs.push @hpricot.get_method_def(index, :instance_method)
        end
        
        if element.fits_the_description?('h2', 'Delegate Methods')
          busy_with = 'Delegate Methods'
        end
        if busy_with == 'Delegate Methods' and @hpricot.start_of_method_def?(index)
          class_def.delegate_method_defs.push @hpricot.get_method_def(index, :instance_method)
        end
        
        if class_def.type == :functions and element.fits_the_description?('h2', 'Functions')
          busy_with = 'Functions'
        end
        if busy_with == 'Functions' and @hpricot.start_of_method_def?(index)
          class_def.function_defs.push @hpricot.get_methodlike_function_def(index)
        end
        
        if class_def.type == :data_types and element.fits_the_description?('h2', 'Data Types')
          busy_with = 'Data Types'
        end
        if busy_with == 'Data Types' and @hpricot.start_of_method_def?(index)
          datatype = @hpricot.get_methodlike_datatype_def(index)
          class_def.datatype_defs.push datatype unless datatype.nil?
        end
          
        if element.fits_the_description?('h2', 'Notifications')
          busy_with = 'Notifications'
        end
        if busy_with == 'Notifications' and @hpricot.start_of_method_def?(index)
          class_def.notification_defs.push @hpricot.get_notification_def(index)
        end

        if element.fits_the_description?('h2', 'Constants')
          busy_with = 'Constants'
        end
        if busy_with == 'Constants' and class_def.type != :constants and @hpricot.start_of_method_def?(index)
          constants = @hpricot.get_constant_defs(index)
          class_def.constant_defs.concat(constants) unless constants.nil?
        end
        # There are also constant descriptions which are more like method descriptions
        if busy_with == 'Constants' and class_def.type != :constants and @hpricot.start_of_method_def?(index)
          constant = @hpricot.get_methodlike_constant_def(index)
          if not constant.discussion.empty? and not constant.availability.empty?
            class_def.constant_defs.push constant
          end
        end
        
        # For the constants in the misc dir there are several types of constants,
        # we probably want to use this as different sections.
        if busy_with == 'Constants' and class_def.type == :constants
          if element.fits_the_description?('h3', 'Enumerations')
            constant_type = :enumeration
          elsif element.fits_the_description?('h3', 'Global Variables')
            constant_type = :global
          elsif element.fits_the_description?('h3', 'Errors')
            constant_type = :error
          elsif element.fits_the_description?('h3', 'Notifications')
            constant_type = :notification
          elsif element.fits_the_description?('h3', 'Exceptions')
            constant_type = :exception
          end
        end
        # Then there are the constants defined in the misc dir, which are a bit different
        if busy_with == 'Constants' and class_def.type == :constants and @hpricot.start_of_framework_constant_def?(index)
          constants = @hpricot.get_constant_defs(index, constant_type)
          class_def.constant_defs.concat(constants) unless constants.nil?
        elsif busy_with == 'Constants' and class_def.type == :constants and @hpricot.start_of_method_def?(index)
          constants = @hpricot.get_constant_defs(index)
          class_def.constant_defs.concat(constants) unless constants.nil?
        end
        
        index = index.next
      end
      
      # Check if we got a name for the class
      if class_def.name.empty?
        error_str = "[ERROR] A empty string was returned as the class name\n"
        @log.add(error_str)
      end
      
      # Remove duplicate constants 
      class_def.constant_defs.uniq!
      
      class_def.create_syntactic_sugar_methods!
      
      return class_def
    end
    
    def to_rb_file(dir)
      file = File.join(dir, @class_def.output_filename)
      File.open(file, 'w') {|f| f.write @class_def.to_rdoc}
    end
    
  end
end
