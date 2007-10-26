module NSStringOverrides
  def new_definition
    # All the args are stringed together (so no spaces) in the original ref
    
    case @name
    when 'initWithContentsOfURL:encoding:error:'
      return '- (id)initWithContentsOfURL:(NSURL *)url encoding:(NSStringEncoding)enc error:(NSError **)error'
    when 'initWithContentsOfURL:usedEncoding:error:'
      return '- (id)initWithContentsOfURL:(NSURL *)url usedEncoding:(NSStringEncoding *)enc error:(NSError **)error'
    end
  end
end