# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  ri_entry.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 FUJIMOTO Hisa. All rights reserved.
#
require 'rdoc/ri/ri_writer'
require 'rdoc/ri/ri_descriptions'
require 'rdoc/markup/simple_markup/to_flow'
require 'ri_text_display'

module TextDescriptionStuff
  def displayer
    @displayer || @displayer = RiTextDisplay.new
  end
end

module OSXEntry
  def osx?
    (/^OSX::/ =~ full_name) ? true : false
  end
end

class RI::ClassEntry
  include OSXEntry
  include TextDescriptionStuff

  attr_reader :in_class, :class_methods, :instance_methods, :inferior_classes

  def description
    result = nil
    path_names.each do |path|
      path = RI::RiWriter.class_desc_path(path, self)
      desc = File.open(path) {|f| RI::Description.deserialize(f) }
      if result
        result.merge_in(desc)
      else
        result = desc
      end
    end
    result
  end

  def text
    displayer.display_class_info(description, nil)
  end
end

class RI::MethodEntry
  include OSXEntry
  include TextDescriptionStuff

  attr_reader :is_class_method, :in_class

  def description
    path = path_name
    File.open(path) { |f| RI::Description.deserialize(f) }
  end

  def text
    displayer.display_method_info(description)
  end
end
