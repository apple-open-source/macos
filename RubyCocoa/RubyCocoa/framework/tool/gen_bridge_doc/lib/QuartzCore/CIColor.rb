module CIColorOverrides
  def new_definition
    return '- (id)initWithCGColor:(CGColorRef)c' if @name == 'initWithCGColor:'
  end
end