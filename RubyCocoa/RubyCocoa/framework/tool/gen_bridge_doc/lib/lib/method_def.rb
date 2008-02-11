class CocoaRef::MethodDef
  include Extras
  
  attr_accessor :type, :name, :description, :definition, :parameters, :return_value, :discussion, :availability, :see_also
  attr_accessor :is_a_setter_variant, :is_a_question_variant
  attr_reader :log
  
  def initialize
    @methods_debug = false
    @is_a_setter_variant, @is_a_question_variant = false
    @type, @name, @description, @definition, @parameters, @return_value, @discussion, @availability, @see_also = '', '', '', '', '', '', '', '', []
    @log = CocoaRef::Log.new
  end
  
  # begin rubyesque added code
  
  def is_setter?
    @name.scan(/:/).length == 1 && @name != 'set' && @name != 'set:' && @name[0..2] == 'set'
  end
  
  def create_rubyesque_setter_variant
    # setFoo() will become foo=()
    dup = self.dup
    dup.is_a_setter_variant = true
    dup.name = self.ruby_setter_for(@name)
    return dup
  end
  
  def ruby_setter_for(str)
    str[3...4].downcase << str[4..-1]
  end

  def is_question?
    @name != 'is' && @name != 'is:' && @name[0..1] == 'is'
  end
  
  def create_rubyesque_question_variant
    # isFoo() will become foo?
    dup = self.dup
    dup.is_a_question_variant = true
    dup.name = self.ruby_question_for(@name)
    return dup
  end
  
  def ruby_question_for(str)
    str[2...3].downcase << str[3..-1]
  end
  
  # end rubyesque added code

  def to_s
    str  = "METHOD:\n#{self.name}\n"
    str += "TYPE:\n#{@type.to_s}\n"
    str += "DEFINITION:\n#{self.definition.strip_tags}\n"
    str += "DESCRIPTION:\n#{@description.strip_tags}\n"
    str += "PARAMETERS:\n#{@parameters.strip_tags}\n"
    str += "RETURN VALUE:\n#{@return_value.strip_tags}\n"
    str += "DISCUSSION:\n#{@discussion.strip_tags}\n"
    str += "AVAILABILITY:\n#{@availability.strip_tags}\n\n"
    str += "SEE ALSO:\n#{@see_also.strip_tags}\n\n"
    return str
  end

  def to_rdoc(class_name)
    str  = ''
    str += "  # Parameters::   #{@parameters.rdocify}\n"   unless @parameters.empty?
    str += "  # Description::  #{@description.rdocify}\n"  unless @description.empty?
    str += "  # Discussion::   #{@discussion.rdocify}\n"   unless @discussion.empty?
    str += "  # Return value:: #{@return_value.rdocify}\n" unless @return_value.empty?
    str += "  # Availability:: #{@availability.rdocify}\n" unless @availability.empty?
    unless @see_also.empty?
      str += "  # See also::     "
      @see_also.each do |s|
        str += "<tt>#{s.to_rb_def}</tt> " unless s.empty?
      end
      str += "\n"
    end
    
    ruby_style_def = self.to_rb_def
    
    str += "  def #{'self.' if @type == :class_method}#{ruby_style_def}\n"
    str += "    # #{self.definition.gsub(/\n/, ' ').strip_tags.clean_special_chars}\n"
    str += "    #\n"
    
    unless self.is_a_setter_variant or self.is_a_question_variant
      unless ruby_style_def == 'an_error_occurred_while_parsing_method_def!'
        objc_method_style = self.to_objc_method(class_name)
        unless objc_method_style.nil?
          str += "    # This is an alternative way of calling this method:\n"
          objc_method_style.each do |line|
            str += "    #{line}\n"
          end
        end
      end
    end
    
    str += "  end\n\n"
  
    return str
  end
  
  def string_spacer(length)
    spacer = ''
    length.times{ spacer += ' '}
    return spacer
  end
  
  def to_objc_method(class_name)
    method_def_parts = self.parse
    return nil if method_def_parts.nil?
    return nil if method_def_parts.length == 1 and method_def_parts.first.empty?

    longest_name = ''
    method_def_parts.each do |m|
      longest_name = m[:name] if m[:name].length > longest_name.length
    end
    
    objc_method_style = []
    index = 0
    method_def_parts.each do |m|          
      str = "#{m[:name] unless m[:name].nil?}, #{string_spacer(longest_name.length - m[:name].length) unless m[:name] == longest_name}#{m[:arg] unless m[:arg].nil?}"
      if index.zero?
        objc_method_style.push "#{class_name + '.alloc.' if @type == :class_method}objc_send(:#{str}#{(index == method_def_parts.length - 1) ? ')' : ','}"
      elsif index == method_def_parts.length - 1
        spacer = ''
        (class_name.length + 5).times{ spacer += ' '} if @type == :class_method
        objc_method_style.push "#{spacer}          :#{str})"
      else
        spacer = ''
        (class_name.length + 5).times{ spacer += ' '} if @type == :class_method
        objc_method_style.push "#{spacer}          :#{str},"
      end
      index = index.next
    end
    return objc_method_style
  end
  
  def name
    self.override_result(@name, :new_name)
  end
  
  def definition
    self.override_result(@definition, :new_definition)
  end
  
  def regexp_start
    self.override_result("([-+\\s]+\\()([\\w\\s]+)([\\s\\*\\)]+)", :new_regexp_start)
  end
  def regexp_repeater
    self.override_result("(\\w+)(:\\()([\\w\\s]+)([\\(\\w,\\s\\*\\)\\[\\d\\]]+\\)\\)|[\\s\\*\\)\\[\\d\\]]+)(\\w+)", :new_regexp_repeater)
  end
  
  def regexp_result_name(res, part)
    self.override_result(res[(part * 6) + 3], :new_regexp_result_name, [res, part])
  end
  def regexp_result_type(res, part)
    self.override_result(res[(part * 6) + 5], :new_regexp_result_type, [res, part])
  end
  def regexp_result_arg(res, part)
    self.override_result(res[(part * 6) + 7], :new_regexp_result_arg, [res, part])
  end
  
  def parse
    parsed_method_name = self.name.split(':')
  
    regexp = regexp_start
    method_def_parts = []
    parsed_method_name.length.times do
      method_def_parts.push regexp_repeater
    end
    regexp += method_def_parts.join("(\\s+)")
    puts regexp if @methods_debug

    parsed_method_def = self.definition.clean_objc.scan(Regexp.new(regexp)).flatten
    p parsed_method_def if @methods_debug

    method_def_parts = []
    parsed_method_name.length.times do |i|
      method_def_part = {}
      md_name = regexp_result_name(parsed_method_def, i)
      method_def_part[:name] = md_name.strip unless md_name.nil?
      
      md_type = regexp_result_type(parsed_method_def, i)
      method_def_part[:type] = md_type.strip unless md_type.nil?
      
      md_arg = regexp_result_arg(parsed_method_def, i)
      unless md_arg.nil?
        method_def_part[:arg] = md_arg.strip.gsub(/class/, 'klass') 
        method_def_part[:arg].sub!(/^[A-Z]/) { |s| s.downcase }
      end
      method_def_parts.push(method_def_part)
    end
    p method_def_parts if @methods_debug
    return method_def_parts
  end
  
  def to_rb_def
    puts @definition.clean_objc if @methods_debug
  
    parsed_method_name = self.name.split(':')
    p parsed_method_name if @methods_debug
  
    if self.definition.strip_tags.include?(':') and not self.definition.strip_tags[-2...-1] == ':'
      method_def_parts = self.parse
      str = method_def_parts.collect {|m| m[:name]}.join('_')
      str += '=' if self.is_a_setter_variant
      str += '(' << method_def_parts.collect{|m| m[:arg] }.join(', ') << ')'
    else
      str = parsed_method_name.join('_')
      str += '?' if self.is_a_question_variant
    end

    if str =~ /^[_(]+/
      error_str  = "[WARNING] A empty string was returned as the method definition for:\n"
      error_str += "          #{@name}\n"
      @log.add(error_str)
      return 'an_error_occurred_while_parsing_method_def!'
    else
      if self.is_a_setter_variant
        return self.ruby_setter_for(str)
      else
        return str
      end
    end
  end
end
