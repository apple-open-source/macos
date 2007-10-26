vp_spec(
  :superMenuTitle => "R Word Count",
       :menuTitle => "R In Page" )

vp_action do  |windowController|

  textView  = windowController.textView
  text      = textView.textStorage.string
  sc        = OSX::NSSpellChecker.sharedSpellChecker
  wordCount = sc.countWordsInString_language(text, sc.language)

  lText       = text.to_s.gsub(/\r\n/, "\n").gsub(/\r/, "\n")

  letterCount = lText.split(//).size
  lineCount   = lText.split("\n").size

  s = format("%d words.\n%d lines.\n%d letters.", 
             wordCount, lineCount, letterCount)
  OSX.NSRunAlertPanel("Word Count", s, nil, nil, nil)
end
