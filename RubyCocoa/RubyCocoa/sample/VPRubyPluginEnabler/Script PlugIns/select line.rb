vp_spec(
  :superMenuTitle => "R Select",
  :menuTitle      => "R Current Line",
  :shortcutKey    => 'l',
  :shortcutMask   => [ :command, :control ] )

vp_action do |windowController|
  windowController.textView.selectLine(nil)
end
