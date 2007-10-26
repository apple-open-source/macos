module PDFPageOverrides
  def new_definition
    return '- (void)setBounds:(NSRect)bounds forBox:(PDFDisplayBox)box' if @name == 'setBounds:forBox:'
  end
end