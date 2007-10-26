# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  ReplController.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
$KCODE = 'utf-8'
require 'osx/cocoa'
require 'evaluator'
require 'io'

class ReplController < OSX::NSObject
  include OSX

  FONTSIZE = 18
  ERR_COLOR = NSColor.redColor
  OK_COLOR  = NSColor.blueColor
  SCRATCH_PATH = File.join(ENV['HOME'], ".cocoarepl_scratch.rb")

  ib_outlets :scratchText, :resultText, :outText, :tabView, :statusView
  ib_outlets :alphaSlider, :window
  ib_outlets :wordsTable, :descriptionText

  # notification receiver for Evaluator
  def evaluated(sender, key, result)
    show_result(result) if result
  end

  # notification receiver for STDOUT/STDERR
  def io_write(sender, key, string)
    color = case sender
            when STDOUT then OK_COLOR
            when STDERR then ERR_COLOR
            else             NSColor.blackColor
            end
    length = @outText.textStorage.length
    range = NSRange.new(length, 0)
    @outText.replaceCharactersInRange_withString(range, string)
    range.length = @outText.textStorage.length - length
    @outText.setTextColor_range(color, range)
    @outText.setSelectedRange(range)
    @outText.scrollRangeToVisible(range)
    @tabView.selectTabViewItemWithIdentifier('output')
  end

  def awakeFromNib
    Evaluator.instance.add_observer(self, :evaluated)
    STDOUT.add_observer(self, :io_write)
    STDERR.add_observer(self, :io_write)
    DecoratorStyle.font_size = FONTSIZE
    font = NSFont.userFixedPitchFontOfSize(FONTSIZE)
    @scratchText.setFont(font)
    @resultText.setFont(font)
    @outText.setFont(font)
    @descriptionText.setFont(NSFont.userFixedPitchFontOfSize(14))

    initial_msg = "Ruby #{RUBY_VERSION}"
    initial_msg << ", RubyCocoa #{RUBYCOCOA_VERSION}"
    initial_msg << " (r#{RUBYCOCOA_SVN_REVISION})"
    @statusView.setStringValue("ready")
    @window.setTitle("RubyCocoa REPL : #{initial_msg}")

    tvdel = RubyProgramTextViewDelegate.alloc.init
    tvdel.setController(self)
    @scratchText.setDelegate(tvdel)
    @wordsTable.setDelegate(@scratchText.delegate)
    @wordsTable.setDataSource(@scratchText.delegate)
    load_scratch
    load_defaults_all
  end

  ib_action :selectScratchView do
    @window.makeFirstResponder(@scratchText)
  end

  ib_action :alphaChanged do |sender|
    @window.alphaValue = sender.floatValue
    save_defaults(DEFAULTS_KEY_ALPHA, @window.alphaValue)
  end

  ib_action :tabChanged do |sender|
    case sender.tag.to_i
    when 1 then @tabView.selectTabViewItemWithIdentifier('result')
    when 2 then @tabView.selectTabViewItemWithIdentifier('output')
    when 3 then @tabView.selectTabViewItemWithIdentifier('reference')
    end
  end

  ib_action :evaluate do |sender|
    @tabView.selectTabViewItemWithIdentifier('result')
    src = string_for_eval(sender.tag)
    Evaluator.evaluate(src)
    nil
  end

  ib_action :complete do
    @scratchText.complete(self)
  end

  ib_action :selectBlock do
    @scratchText.selectCurrentBlock
  end

  def reloadWordsTable
    @wordsTable.reloadData
    @tabView.selectTabViewItemWithIdentifier('reference')
  end

  def showDescription(str)
    @descriptionText.setString(str)
    @tabView.selectTabViewItemWithIdentifier('reference')
  end

  private

  def string_for_eval(tag)
    storage = @scratchText.textStorage
    case tag
    when 0 then storage.rubyString
    when 1 then storage.rubySubString(@scratchText.selectedRange)
    when 2 then storage.rubySubString(@scratchText.rangeForCurrentLine)
    when 3 then storage.rubySubString(@scratchText.rangeForCurrentBlock)
    end
  end

  DEFAULTS_KEY_ALPHA = 'CocoaReplMainWindowAlphaValue'

  def save_defaults(key, val)
    NSUserDefaults.standardUserDefaults.setObject_forKey(val, key)
  end

  def load_defaults(key, default=nil, &blk)
    val = NSUserDefaults.standardUserDefaults.objectForKey(key) || default
    yield(val) if block_given?
    val
  end

  def load_defaults_all
    @alphaSlider.floatValue =
      @window.alphaValue = load_defaults(DEFAULTS_KEY_ALPHA, 0.9)
  end

  def save_defaults_all
    save_defaults(DEFAULTS_KEY_ALPHA, @window.alphaValue)
  end

  def store_scratch
    str = @scratchText.textStorage.rubyString
    str.strip!
    File.open(SCRATCH_PATH,"w") { |io| io.write(str) ; io.puts } unless str.empty?
  end

  def load_scratch
    if File.exist? SCRATCH_PATH then
      @scratchText.setString(File.read(SCRATCH_PATH))
      @scratchText.decorate_all
      @scratchText.didChangeText
    end
  end

  def show_result(result)
    store_scratch
    save_defaults_all
    if err = result.error then
      @statusView.setStringValue(err.message[0,100])
      info = "#{err.class}: #{err.message}\n"
      err.backtrace.each { |i| info << "  #{i}\n" }
      @resultText.setString(info)
      @resultText.setTextColor(ERR_COLOR)
      @tabView.selectTabViewItemWithIdentifier('result')
    else
      @statusView.setStringValue(result.retval.inspect[0,100])
      @resultText.setString(result.retval.inspect)
      @resultText.setTextColor(OK_COLOR)
    end
  end
end
