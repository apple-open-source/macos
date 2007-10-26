vp_spec(
  :menuTitle    => "R New Page With Current Date",
  :shortcutKey  => 'j',
  :shortcutMask => [:command, :control] )

vp_action do |windowController|

  pageName = Time.now.strftime("%Y.%m.%d")
  windowController.document.createNewPageWithName(pageName)
  windowController.textView.insertText(pageName)
  
end
