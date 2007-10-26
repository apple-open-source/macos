# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
require 'osx/cocoa'
OSX.require_framework 'WebKit'

class MyPluginClass < OSX::NSObject

  QUOTES = ["You will be awarded some great honor.",		
            "You are soon going to change your present line of work.",
            "You will have gold pieces by the bushel.",
            "You will be fortunate in the opportunities presented to you.",
            "Someone is speaking well of you.",
            "Be direct, usually one can accomplish more that way.",
            "You need not worry about your future.",
            "Generosity and perfection are your everlasting goals." ]
  
  NUM_QUOTES = QUOTES.size
  
  def initWithWebView(w)
    log("Entering -initWithWebView: %s", w)
    # super_init  (FIXME - never return after calling the super_init in the bundle)
    return self
  end
  
  def windowScriptObjectAvailable(wso)
    log("windowScriptObjectAvailable: %s", wso)
    wso.setValue_forKey(self, "FortunePlugin")
  end

  def self.webScriptNameForSelector(aSel)
    log("webScriptNameForSelector: %s", aSel)
    if is_available_selector?(aSel) then
      return aSel.to_s.sub(/:$/,'')
    else
      log("unknown selector - %s", aSel)
      return nil
    end
  end
  
  def self.isSelectorExcludedFromWebScript(aSel)
    ret = ! is_available_selector?(aSel)
    log("isSelectorExcludedFromWebScript: %s => %s", aSel, ret)
    return ret
  end
  
  def self.isKeyExcludedFromWebScript(k)
    log("isKeyExcludedFromWebScript: %s", k)
    return true
  end
  
  def getFortune
    log("getFortune")
    old = @sayingCount
    @sayingCount = rand(NUM_QUOTES) while old == @sayingCount
    return QUOTES[@sayingCount]
  end
  objc_method :getFortune, "@@:"
  
  def logMessage(str)
    log("JavaScript says: %p", str.to_s)
    return nil
  end
  objc_method :logMessage, "v@:@"
  
  private

  def self.is_available_selector?(sel)
    sels =[ 'getFortune', 'logMessage:' ]
    return sels.include?(sel.to_s)
  end
  
  def self.log(fmt, *args)
    OSX.NSLog("MyPluginClass(ruby): %@", (fmt % args))
  end
  
  def log(fmt, *args)
    MyPluginClass.log(fmt, *args)
  end
  
end
