module PDFSelectionOverrides
  def new_definition
    case @name
      when 'drawForPage:active:'
        return '- (void)drawForPage:(PDFPage *)page active:(BOOL)active'
      when 'drawForPage:withBox:active:'
        return '- (void)drawForPage:(PDFPage *)page withBox:(PDFDisplayBox)box active:(BOOL)active'
    end
  end
end