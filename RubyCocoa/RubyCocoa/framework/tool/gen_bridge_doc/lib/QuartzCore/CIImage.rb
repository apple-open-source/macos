module CIImageOverrides
  def new_name
    case @name
    when 'imageWithImageProvider:size::format:colorSpace:options:'
      return 'imageWithImageProvider:size:format:colorSpace:options:'
    when 'initWithImageProvider:size::format:colorSpace:options:'
      return 'initWithImageProvider:size:format:colorSpace:options:'
    end
  end
  
  def new_definition
    case @name
    when 'imageWithContentsOfURL:'
      return '+ (CIImage *)imageWithContentsOfURL:(NSURL *)url'
    when 'imageWithData:'
      return '+ (CIImage *)imageWithData:(NSData *)data'
    when 'imageWithData:options:'
      return '+ (CIImage *)imageWithData:(NSData *)data options:(NSDictionary *)d'
    when 'imageWithImageProvider:size::format:colorSpace:options:'      
      return '+ (CIImage *)imageWithImageProvider:(id)p size:(size_t)size format:(CIFormat)f colorSpace:(CGColorSpaceRef)cs options:(NSDictionary *)dict'
    when 'initWithImageProvider:size::format:colorSpace:options:'
      return '- (id)initWithImageProvider:(id)p size:(size_t)size format:(CIFormat)f colorSpace:(CGColorSpaceRef)cs options:(NSDictionary *)dict'
    when 'initWithTexture:size:flipped:colorSpace:'
      return '- (id)initWithTexture:(unsigned long)name size:(CGSize)size flipped:(BOOL)flag colorSpace:(CGColorSpaceRef)cs'
    end
  end
end