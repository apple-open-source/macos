module NSTableColumnOverrides
  def new_regexp_repeater
    return "(\\w+)(:)(\\w+)" if @name == 'initWithIdentifier:'
  end
  
  def new_regexp_result_name(res, part)
    return res[(part * 6) + 3] if @name == 'initWithIdentifier:'
  end
  
  def new_regexp_result_type(res, part)
    return 'id' if @name == 'initWithIdentifier:'
  end
  
  def new_regexp_result_arg(res, part)
    return res[(part * 6) + 5] if @name == 'initWithIdentifier:'
  end
end