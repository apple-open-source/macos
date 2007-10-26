module NSFontOverrides
  def new_regexp_start
    return "([-+\\s]+\\()" if @name == 'getAdvancements:forPackedGlyphs:length:'
  end
  
  def new_regexp_result_name(res, part)
    return res[(part * 6) + 1] if @name == 'getAdvancements:forPackedGlyphs:length:'
  end
  
  def new_regexp_result_type(res, part)
    return res[(part * 6) + 3] if @name == 'getAdvancements:forPackedGlyphs:length:'
  end
  
  def new_regexp_result_arg(res, part)
    return res[(part * 6) + 5] if @name == 'getAdvancements:forPackedGlyphs:length:'
  end
end