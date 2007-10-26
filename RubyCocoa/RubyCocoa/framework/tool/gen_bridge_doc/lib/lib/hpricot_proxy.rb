module CocoaRef
  class HpricotProxy
    include Extras
    
    attr_accessor :elements, :exclude_constant_names
    attr_reader :log
  
    def initialize(file)
      @log = CocoaRef::Log.new
      @exclude_constant_names = ['NSString', 'NSSize']
      
      doc = open(file) {|f| Hpricot f, :fixup_tags => true }
      @elements = (doc/"//body/*").select{ |e| e.is_a?(Hpricot::Elem) }
    end
  
    def start_of_method_def?(index)
      @elements[index].name == 'h3' and @elements[index].get_attribute('class') == 'tight' and @elements[index + 1].spacer?
    end
    
    # This is a check for for the start of a constant def
    # as they are used in e.g. Miscallaneous/AppKit_Constants
    def start_of_framework_constant_def?(index)
      @elements[index].name == 'h4' and @elements[index + 1].spacer?
    end
    
    def get_method_def(index, type)
      method_def = CocoaRef::MethodDef.new
      method_def.type         = type
      method_def.name         = self.get_name_for_method(index)
      method_def.description,  new_index = self.get_description_for_method(index)
      method_def.definition,   new_index = self.get_definition_for_method(new_index)
      method_def.parameters,   new_index = self.get_parameters_for_method(new_index)
      method_def.return_value, new_index = self.get_return_value_for_method(new_index)
      method_def.discussion,   new_index = self.get_discussion_for_method(new_index)
      method_def.availability, new_index = self.get_availability_for_method(new_index)
      method_def.see_also,     new_index = self.get_see_also_for_method(new_index)
      return method_def
    end
  
    def get_name_for_method(index)
      @elements[index].inner_html
    end
  
    def method_def_has_description?(index)
      @elements[index + 1].spaceabove?
    end
  
    def get_description_for_method(index)
      if self.method_def_has_description?(index)
        elm_index = find_next_tag('p', 'spaceabove', index)
        elm = @elements[elm_index]
        return [elm.inner_html, elm_index + 1]
      else
        return '', index
      end
    end
  
    def get_definition_for_method(index)
      elm_index = find_next_tag('p', 'spaceabovemethod', index)
      unless elm_index.nil?
        elm = @elements[elm_index]
        return [elm.inner_html, elm_index + 1]
      else
        return ['', index]
      end
    end
  
    def get_h5_section_for_method(index, search)
      elm_index = find_next_tag('h5', 'tight', index)
      unless elm_index.nil?
        elm = @elements[elm_index]
        if elm.inner_html == search
          return [@elements[index + 1].inner_html, elm_index + 2]
        end
      end
      return ['', index]
    end
  
    # When passed the index of a paragraph,
    # this method will try to retrieve the paragraph and any subsequent paragraphs.
    # It also returns the index of the last found paragraph.
    def get_the_text(index)
      str = ''
      arr = []
      while @elements[index].name == 'p'
        arr.push @elements[index].inner_html
        index = index.next
      end
      arr.delete_if {|paragraph| paragraph.strip.empty? }
      return [arr, index]
    end
  
    def get_parameters_for_method(index)
      get_h5_section_for_method(index, 'Parameters')
    end
  
    def get_return_value_for_method(index)
      get_h5_section_for_method(index, 'Return Value')
    end
  
    def get_discussion_for_method(index)
      get_h5_section_for_method(index, 'Discussion')
    end
  
    def get_availability_for_method(index)
      get_h5_section_for_method(index, 'Availability')
    end
  
    def get_see_also_for_method(index)
      #get_h5_section_for_method(index, 'See Also')
      elm_index = find_next_tag('h5', 'tight', index)
      unless elm_index.nil?
        elm = @elements[elm_index]
        if elm.inner_html == 'See Also'
          children = @elements[elm_index + 1].children
          see_also_items = []
          children.each do |c|
            see_also_items.push(c.inner_html) if c.is_a?(Hpricot::Elem)
          end
          return [see_also_items, elm_index + 2]
        end
      end
      return [[], index]
    end
  
    def get_methodlike_constant_def(index)
      constant_def = CocoaRef::ConstantDef.new
      constant_def.name         = self.get_name_for_method(index)
      constant_def.description,  new_index = self.get_description_for_method(index)
      constant_def.discussion,   new_index = self.get_discussion_for_method(new_index)
      constant_def.availability, new_index = self.get_availability_for_method(new_index)
      return constant_def
    end
  
    def get_constant_defs(index, type = :normal)
      constants, new_elm_index = self.get_names_and_values_for_constants(index)
      constants_with_desc_and_avail, new_elm_index = self.get_descriptions_and_availability_for_constants(new_elm_index, constants)
      constants = constants_with_desc_and_avail unless constants_with_desc_and_avail.nil?
      
      unless constants.nil?
        constant_defs = []
        constants.each do |c|
          constant_def = CocoaRef::ConstantDef.new
          constant_def.name          = c[:name]
          constant_def.type          = type unless type == :normal # Only add types from the misc dir constants
          constant_def.value         = c[:value]
          constant_def.description   = c[:description]
          constant_def.availability  = c[:availability]
          constant_defs.push constant_def
        end
      else
        warning_str  = "[WARNING] A `nil` value was detected in the constants section...\n"
        warning_str += "          It might be a bug, please cross check the original reference with the output.\n"
        puts warning_str if $COCOA_REF_DEBUG
      end
      
      return constant_defs
    end
    
    def get_names_and_values_for_constants(index)
      elm_index = find_next_tag('pre', '', index)
      children = @elements[elm_index].children
      names_and_values = []
      elm_index = elm_index.next
      children_index = 0
      last_excluded_constant_name = ''
      until children_index > (children.length - 1) do
        elm = children[children_index]
        
        if elm.is_a?(Hpricot::Elem) and elm.name == 'a' and @exclude_constant_names.include?(elm.inner_html)
          last_excluded_constant_name = "OSX::#{elm.inner_html}"
        end
        
        if elm.is_a?(Hpricot::Elem) and elm.name == 'a' and not @exclude_constant_names.include?(elm.inner_html)
          names_and_values.push({:name => elm.inner_html, :value => last_excluded_constant_name})

        # This is a check for constant names that are not the children of a <a> tag
        elsif elm.is_a?(Hpricot::Comment) and elm.to_s == '<!--a-->'
          names_and_values.push({:name => children[children_index + 1].to_s, :value => last_excluded_constant_name})
          
        elsif elm.is_a?(Hpricot::Text) and elm.to_s =~ /\w+;/ and not elm.to_s.include?('}')
          parsed_name = elm.to_s.scan(/(\w+)(;)/).flatten.first
          names_and_values.push({:name => parsed_name, :value => last_excluded_constant_name})
        
        elsif elm.is_a?(Hpricot::Text) and elm.to_s.include?('define')
          elms = elm.to_s.split("\n")
          elms.delete("")
          elms.each do |e|
            parsed_name = e.to_s.scan(/(define\s+)(\w+)/).flatten.last
            names_and_values.push({:name => parsed_name, :value => ''})
          end
          
        elsif elm.is_a?(Hpricot::Text) and elm.to_s.include?('=')
          # First we check if we have arrived at a piece of text that only holds
          # a equal sign, but not the value itself
          if elm.to_s =~ /=\s+$/
            # If so, then it's probably a <!--a--> comment tag,
            # so move the index up 2 elements where the text should be.
            children_index = children_index + 2
            parsed_value = children[children_index].to_s
          else
            # Parse the text that should contain the equal sign and the value itself.
            parsed_value = elm.to_s.gsub(/&lt;/, '<').scan(/([\s=]+)([\(\-_\w\d\)<>\s]+)/).flatten[1]
          end
          unless names_and_values.last.nil? or names_and_values.last.empty?
            names_and_values.last[:value] = parsed_value
          else
            error_str  = "[WARNING] A value was found without a matching name in the constants section...\n"
            error_str += "          It might be a bug, please cross check the original reference with the output.\n"
            error_str += "          Value: #{parsed_value[1]}\n"
            @log.add(error_str)
          end
        end
        children_index = children_index.next
        elm_index = elm_index.next
      end
      return [names_and_values, index + 1]
    end
  
    def get_descriptions_and_availability_for_constants(index, constants)
      # For some reason the dl tag where these constants
      # are the children of comes back empty...
      # So that's why we have this lengthy method.
    
      last_good_index = index
    
      # First let's get an array of the constant names of this section,
      # this is what we check against to see if the description belongs to a constant.
      names = constants.collect {|c| c[:name]}
    
      # Find the first constant description for this section
      elm_index = find_next_tag('dt', '', index)
      return nil if elm_index.nil?
      elm = @elements[elm_index]
    
      # Loop until we come across a constant which doesn't belong to this section.
      while names.include? elm.inner_html.strip_tags
        # Get a reference to the constant that we are processing
        constant_index = names.index(elm.inner_html.strip_tags)
      
        # Find the description that belongs to the constant
        elm_index = find_next_tag('dd', '', elm_index)
        elm = @elements[elm_index]
      
        # Add the description to it's constant
        constants[constant_index][:description] = elm.children.first.inner_html
        # Add the availability to it's constant
        constants[constant_index][:availability] = elm.children.last.inner_html
      
        # Get a reference to the last known good elm_index
        last_good_index = elm_index
      
        # Find the next start of an constant section
        elm_index = find_next_tag('dt', '', elm_index)
        break if elm_index.nil?
        elm = @elements[elm_index]
      end
      return [constants, last_good_index + 1]
    end
  
    def get_notification_def(index)
      notification_def = CocoaRef::NotificationDef.new
      notification_def.name         = self.get_name_for_method(index)
      notification_def.description,  new_index = self.get_description_for_method(index)
      notification_def.definition,   new_index = self.get_definition_for_method(new_index)
      notification_def.parameters,   new_index = self.get_parameters_for_method(new_index)
      notification_def.return_value, new_index = self.get_return_value_for_method(new_index)
      notification_def.discussion,   new_index = self.get_discussion_for_method(new_index)
      notification_def.availability, new_index = self.get_availability_for_method(new_index)
      notification_def.see_also,     new_index = self.get_see_also_for_method(new_index)
      return notification_def
    end
  
    def get_methodlike_function_def(index)
      function_def = CocoaRef::FunctionDef.new
      function_def.name         = self.get_name_for_method(index)
      function_def.description,  new_index = self.get_description_for_method(index)
      function_def.definition,   new_index = self.get_definition_for_function_or_datatype(new_index)
      function_def.parameters,   new_index = self.get_parameters_for_method(new_index)
      function_def.return_value, new_index = self.get_return_value_for_method(new_index)
      function_def.discussion,   new_index = self.get_discussion_for_function_or_datatype(new_index)
      function_def.availability, new_index = self.get_availability_for_method(new_index)
      function_def.see_also,     new_index = self.get_see_also_for_method(new_index)
      return function_def
    end
  
    def get_definition_for_function_or_datatype(index)
      elm_index = find_next_tag('pre', '', index)
      unless elm_index.nil?
        elm = @elements[elm_index]
        return [elm.inner_html, elm_index + 1]
      else
        return ['', index]
      end
    end
  
    def get_discussion_for_function_or_datatype(index)
      elm_index = find_next_tag('h5', 'tight', index)
      unless elm_index.nil?
        elm = @elements[elm_index]
        if elm.inner_html == 'Discussion'
          return self.get_the_text(index + 1)
        end
      end
      return ['', index]
    end
    
    def get_methodlike_datatype_def(index)
      name = self.get_name_for_method(index)
      # First check if this is one of the datatypes that we should process,
      # since we only need to do Boxed types.
      begin
        OSX.const_get(name).ancestors.include?(OSX::Boxed)
      rescue NameError
        # Here we can output the name of the data structure that we skipped.
        # Laurent wanted this to debug BridgeSupport.
        #puts name
        return nil
      end
      
      datatype_def = CocoaRef::DataTypeDef.new
      datatype_def.name = name
      datatype_def.description,  new_index = self.get_description_for_method(index)
      datatype_def.definition,   new_index = self.get_definition_for_function_or_datatype(new_index)
      datatype_def.fields,       new_index = self.get_fields_for_dataype(new_index)
      datatype_def.discussion,   new_index = self.get_discussion_for_function_or_datatype(new_index)
      datatype_def.availability, new_index = self.get_availability_for_method(new_index)
      datatype_def.see_also,     new_index = self.get_see_also_for_method(new_index)
      return datatype_def
    end
    
    def get_fields_for_dataype(index)
      get_h5_section_for_method(index, 'Fields')
    end
    
    def find_next_tag(tag, css_class, start_from = 0)
      found = false
      index = start_from
      until found
        break if @elements[index].nil?
        if @elements[index].name == tag
          unless css_class.empty?
            if @elements[index].get_attribute("class") == css_class
              found = true
            else
              index = index.next
            end
          else
            found = true
          end
        else
          index = index.next
        end
      end
      if found
        return index
      else
        return nil
      end
    end
  
  end
end