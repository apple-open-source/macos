module NSArrayOverrides
  def new_definition
    case @name
    when 'removeObserver:forKeyPath:'
      return '- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath'
    when 'addObserver:forKeyPath:options:context:'
      return '- (void)addObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context'
    end
  end
end