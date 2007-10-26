class CocoaRef::FunctionDef < CocoaRef::MethodDef
  def to_rdoc
    str  = ''
    str += "  # Description::  #{@description.rdocify}\n"  unless @description.empty?
    unless @discussion.empty?
      index = 0
      @discussion.each do |paragraph|
        if index.zero?
          str += "  # Discussion::   #{paragraph.rdocify}\n"
        else
          str += "  #                #{paragraph.rdocify}\n"
        end
        str += "  #\n"
        index = index.next
      end
    end
    str += "  # Availability:: #{@availability.rdocify}\n" unless @availability.empty?
    unless @see_also.empty?
      str += "  # See also::     "
      @see_also.each do |s|
        str += "<tt>#{s.to_rb_def}</tt> " unless s.empty?
      end
      str += "\n"
    end
    str += "  def self.#{self.to_rb_def}\n"
    str += "    # #{self.definition.gsub(/\n/, ' ').strip_tags.clean_special_chars}\n"
    str += "    #\n"
  
    str += "  end\n\n"
  
    return str
  end
  
  def to_rb_def
    #puts @definition.clean_objc
  
    function_def_parts = self.parse
    str = "#{@name}(#{function_def_parts.collect {|f| f[:arg] }.join(', ')})"
  
    if str =~ /^[_(]+/
      error_str  = "[WARNING] A empty string was returned as the method definition for:\n"
      error_str += "          #{@name}\n"
      @log.add(error_str)
      return 'an_error_occurred_while_parsing_method_def!'
    else
      return str
    end
  end
  
  def regexp_start
    self.override_result('(\()(.+)(\))', :new_regexp_start)
  end
  def regexp_repeater
    self.override_result('^(.+)\s+([^\s]+)\s*$', :new_regexp_repeater)
  end
  
  def regexp_result_arg(res)
    self.override_result(res.last.gsub(/\*/, ''), :new_regexp_result_arg, [res])
  end
  def regexp_result_type(res)
    self.override_result(res.first.gsub(/\*/, ''), :new_regexp_result_type, [res])
  end
  
  def parse
    args_part = self.definition.clean_objc.scan(Regexp.new(self.regexp_start)).flatten[1]  
    return [] if args_part.nil?
    
    args = args_part.split(', ')
    function_def_parts = []
    if args.length == 1 and args.first == 'void'
      return function_def_parts
    else
      args.each do |a|
        if a.split("\s").length > 1
          parsed_arg = a.scan(Regexp.new(self.regexp_repeater)).flatten
          #p parsed_arg

          unless parsed_arg.empty?
            function_def_part = {}
            function_def_part[:arg]  = regexp_result_arg(parsed_arg)
            function_def_part[:type] = regexp_result_type(parsed_arg)
            function_def_parts.push function_def_part
          else
            puts "[WARNING] A empty string was returned as a argument to function:\n" if $COCOA_REF_DEBUG
            puts "          #{@name}\n" if $COCOA_REF_DEBUG
          end
        else
          function_def_part = {}
          function_def_part[:arg]  = a
          function_def_part[:type] = 'id'
          function_def_parts.push function_def_part
        end
      end

      #p function_def_parts
      function_def_parts.each do |p|
        if p[:arg] == '...'
          p[:arg] = '*args'
        else
          p[:arg].sub!(/^[A-Z]/) { |s| s.downcase }
          p[:arg].sub!(/\[\d*\]/, '')
        end
      end 
      return function_def_parts
    end
  end
end
