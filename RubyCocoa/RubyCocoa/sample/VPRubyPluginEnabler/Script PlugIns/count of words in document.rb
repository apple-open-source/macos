vp_spec(
  :superMenuTitle => "R Word Count",
       :menuTitle => "R In Document" )

vp_action do |windowController|

  totalPageCount  = 0
  totalWordCount  = 0

  sc = OSX::NSSpellChecker.sharedSpellChecker
  document = windowController.document

  document.keys.each do |pageKey|
    page = document.pageForKey(pageKey)
    
    # voodoopad 3.0 supports multiple types of pages.  So we make sure
    # it's a regular page type first.
    # if page.oc_type == OSX::VPPageType  then
    if page.oc_type.to_s == 'page'  then
      attString      = page.dataAsAttributedString
      pageText       = attString.string
      totalWordCount += sc.countWordsInString_language(pageText, sc.language)
      totalPageCount += 1
    end
  end

  s = format("There are %d words in %d pages", totalWordCount, totalPageCount)
  OSX.NSRunAlertPanel("Document word count", s, nil, nil, nil)
end
