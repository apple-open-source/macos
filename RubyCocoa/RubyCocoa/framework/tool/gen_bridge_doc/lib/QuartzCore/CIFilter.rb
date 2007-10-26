module CIFilterOverrides
  def new_definition
    return '+ (CIFilter *)filterWithName:(NSString *)name keysAndValues:(id)keys' if @name == 'filterWithName:keysAndValues:'
  end
end