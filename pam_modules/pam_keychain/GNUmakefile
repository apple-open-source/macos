##
# pam_keychain Makefile
##
# Luke Howard <lukeh@padl.com>
##

DEBUG                    = YES

PAMModuleName            = pam_keychain

Extra_CC_Flags           = -I/System/Library/Frameworks/Security.framework/PrivateHeaders 
Extra_LD_Flags           = -framework Security -lpam -lpam_misc

include ../PAMModule.defs
