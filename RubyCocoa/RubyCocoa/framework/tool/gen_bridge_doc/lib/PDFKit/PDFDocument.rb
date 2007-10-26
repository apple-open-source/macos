module PDFDocumentOverrides
  def new_definition
    case @name
      when 'findString:fromSelection:withOptions:'
        return '- (PDFSelection *)findString:(NSString *)string fromSelection:(PDFSelection *)selection withOptions:(int)options'
      when 'writeToFile:withOptions:'
        return '- (BOOL)writeToFile:(NSString *)path withOptions:(NSDictionary *)options'
      when 'writeToURL:withOptions:'
        return '- (BOOL)writeToURL:(NSURL *)url withOptions:(NSDictionary *)options'
    end
  end
end