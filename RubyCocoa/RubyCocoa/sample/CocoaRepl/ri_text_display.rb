# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  ri_text_display.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'stringio'
require 'rdoc/ri/ri_display'
require 'rdoc/ri/ri_formatter'
require 'rdoc/ri/ri_options'

class  RiTextDisplay < DefaultDisplay

  def initialize(format = 'plain')
    @options = RI::Options.instance
    @options.parse([ '-f', format ])
    @formatter = @options.formatter.new(@options, "     ")
  end

  def display_class_info(klass, ri_reader)
    page do
      superclass = klass.superclass_string

      if superclass
        superclass = " < " + superclass
      else
        superclass = ""
      end

      @formatter.draw_line(klass.display_name + ": " +
                           klass.full_name + superclass)

      display_flow(klass.comment)
      @formatter.draw_line

      unless klass.includes.empty?
        @formatter.blankline
        @formatter.display_heading("Includes:", 2, "")
        incs = []
        klass.includes.each do |inc|
          inc_desc = ri_reader && ri_reader.find_class_by_name(inc.name)
          if inc_desc
            str = inc.name + "("
            str << inc_desc.instance_methods.map{|m| m.name}.join(", ")
            str << ")"
            incs << str
          else
            incs << inc.name
          end
        end
        @formatter.wrap(incs.sort.join(', '))
      end

      unless klass.constants.empty?
        @formatter.blankline
        @formatter.display_heading("Constants:", 2, "")
        len = 0
        klass.constants.each { |c| len = c.name.length if c.name.length > len }
        len += 2
        klass.constants.each do |c|
          @formatter.wrap(c.value,
                          @formatter.indent+((c.name+":").ljust(len)))
        end
      end

      unless klass.class_methods.empty?
        @formatter.blankline
        @formatter.display_heading("Class methods:", 2, "")
        @formatter.wrap(klass.class_methods.map{|m| m.name}.sort.join(', '))
      end

      unless klass.instance_methods.empty?
        @formatter.blankline
        @formatter.display_heading("Instance methods:", 2, "")
        @formatter.wrap(klass.instance_methods.map{|m| m.name}.sort.join(', '))
      end

      unless klass.attributes.empty?
        @formatter.blankline
        @formatter.wrap("Attributes:", "")
        @formatter.wrap(klass.attributes.map{|a| a.name}.sort.join(', '))
      end
    end
  end

  private

  def page
    @save_stdout = $stdout
    $stdout = io = StringIO.new
    yield
    io.string
  ensure
    $stdout = @save_stdout
  end
end
