module PDFViewOverrides
  def new_definition
    case @name
    when 'selectAll:'
      return '- (IBAction)selectAll:(id)sender'
    when 'printWithInfo:autoRotate:'
      return '- (void)printWithInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate'
    when 'allowsDragging'
      return '- (BOOL)allowsDragging'
    end
  end
end