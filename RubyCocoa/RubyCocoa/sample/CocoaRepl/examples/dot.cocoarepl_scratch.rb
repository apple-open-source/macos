include OSX
require_framework "QuartzComposer"
w = NSWindow.create
v = w.contentView = QCView.create
v.objc_methods.grep /^load/i
v. loadCompositionFromFile "/Developer/Examples/Quartz Composer/Motion Graphics Compositions/Cube Replicator.qtz"
v.startRendering
w.zoom(self)
