module CIContextOverrides
  def new_definition
    case @name
    when 'contextWithCGContext:options:'
      return '+ (CIContext *)contextWithCGContext:(CGContextRef)ctx options:(NSDictionary *)dict'
    when 'contextWithCGLContext:pixelFormat:options:'
      return '+ (CIContext *)contextWithCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf options:(NSDictionary *)dict'
    end
  end
end