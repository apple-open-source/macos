module CIImageProviderOverrides
  def new_name
    return 'provideImageData:bytesPerRow:origin:size:userInfo:' if @name == 'provideImageData:bytesPerRow:origin::size::userInfo:'
  end
  
  def new_definition
    return '- (void)provideImageData:(void *)data bytesPerRow:(size_t)rowbytes origin:(size_t)origin size:(size_t)size userInfo:(id)info' if @name == 'provideImageData:bytesPerRow:origin::size::userInfo:'
  end
end