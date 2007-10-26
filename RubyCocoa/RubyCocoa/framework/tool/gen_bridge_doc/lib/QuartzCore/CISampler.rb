module CISamplerOverrides
  def new_definition
    case @name
    when 'samplerWithImage:keysAndValues:'
      return '+ (CISampler *)samplerWithImage:(CIImage *)im keysAndValues:(id)keys'
    when 'initWithImage:'
      return '- (id)initWithImage:(CIImage *)im'
    when 'initWithImage:keysAndValues:'
      return '- (id)initWithImage:(CIImage *)im keysAndValues:(id)keys'
    when 'initWithImage:options:'      
      return '- (id)initWithImage:(CIImage *)im options:(NSDictionary *)dict'
    end
  end
end