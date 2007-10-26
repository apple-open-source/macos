# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  ruby_anywhere_init.rb
#  RubyAnywhere
#
#  Created by Fujimoto Hisa on 07/02/01.
#  Copyright (c) 2007 FOBJ SYSTEMS. All rights reserved.
#

require 'osx/cocoa'

OSX.init_for_bundle do
  |bdl, param, log|
  # bdl    - the bundle related with the 2nd argument of RBBundleInit
  # param  - the 3rd argument of RBBundleInit as optional data
  # log    - logger for this block

  # log.info("param=%p", param.to_s)

  require 'RcodeController'
  rcode = RcodeController.instance
  OSX::NSBundle.loadNibNamed_owner("Rcode.nib", rcode)
end
