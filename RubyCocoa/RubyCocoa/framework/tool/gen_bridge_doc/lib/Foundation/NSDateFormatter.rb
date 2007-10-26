module NSDateFormatterOverrides
  def new_definition
    return '- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string range:(inout NSRange *)rangep error:(NSError **)error' if @name == 'getObjectValue:forString:range:error:'
  end
end