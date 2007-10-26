vp_spec(
  :superMenuTitle => "R Select",
  :menuTitle      => "R Current Paragraph",
  :shortcutKey    => 'o',
  :shortcutMask   => [ :command, :control ] )

vp_action do |windowController|
  windowController.textView.selectParagraph(nil)
end
