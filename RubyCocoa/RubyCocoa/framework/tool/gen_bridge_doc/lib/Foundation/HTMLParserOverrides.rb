module HTMLParserOverrides
  def self.extended(klass)
    klass.exclude_constant_names += ['NSHashTableCallBacks', 'NSMapTableKeyCallBacks', 'NSMapTableValueCallBacks', 'NSPoint', 'NSRect']
  end
end