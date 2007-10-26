# -*- mode:ruby:indent-tabs-mode:nil; coding:utf-8 -*-
# vim:ts=2:sw=2:expandtab:
require 'test/unit'
require 'fileutils'
require 'pp'
require 'pathname'
require 'iconv'
require 'stringio'

ROOT = (Pathname.new(File.dirname(__FILE__))+"..").realpath

$LOAD_PATH.unshift((Pathname.new(File.dirname(__FILE__))+"../lib").realpath.to_s)
ENV["RUBYLIB"] = "#{ROOT}:#{ENV["RUBYLIB"]}"
ENV["PATH"]    = "#{ROOT}:#{ENV["PATH"]}"

load ROOT + "bin/rubycocoa"
require 'osx/xcode'
include FileUtils

class RubyCocoaCommandTest < Test::Unit::TestCase
  def setup
    @testdir   = ROOT + "test"
  end

  def teardown
    rm_rf 'Test Ruby Cocoa'
  end

  def test_load
    assert true
  end

  def test_create
    create
    assert File.exist?("Test Ruby Cocoa")
    cd 'Test Ruby Cocoa' do
      files = Dir['**/*']
      assert files.include?("Test Ruby Cocoa.xcodeproj"), "Check .xcodeproj"
      assert files.include?("Test Ruby Cocoa.xcodeproj/project.pbxproj"), "Check .pbxproj"
      assert_nothing_raised do
        XcodeProject.new("Test Ruby Cocoa.xcodeproj")
      end
      assert_no_match /«PROJECTNAMEASXML»/, File.read("Info.plist")
      assert_no_match /PROJECTNAME/, Iconv.conv("ISO-8859-1", "UTF-16", File.read("English.lproj/InfoPlist.strings"))
      assert_no_match /PROJECTNAME/, File.read("rb_main.rb")
      assert_no_match /PROJECTNAME/, File.read("main.m")
      assert_no_match /PROJECTNAME/, File.read("Rakefile")
      assert_match /\*\* BUILD SUCCEEDED \*\*/, %x{xcodebuild}
      assert File.exist?("build/Release/Test Ruby Cocoa.app/Contents/Resources/rb_main.rb")
    end
  end

  def test_create_class
    create
    cd 'Test Ruby Cocoa' do
      rubycocoa "create", "AppController"
      assert_match /class AppController < NSObject/, File.read("AppController.rb")

      rubycocoa "create", "-a", "hello", "-o", "hogehoge", "AppController1"
      assert_match /class AppController1 < NSObject/, File.read("AppController1.rb")
      assert_match /ib_action :hello do/, File.read("AppController1.rb")
      assert_match /ib_outlets :hogehoge/, File.read("AppController1.rb")

      rubycocoa "create", "-a", "hello", "-o", "hogehoge", "AppController2<NSWindow"
      assert_match /class AppController2 < NSWindow/, File.read("AppController2.rb")
      assert_match /ib_action :hello do/, File.read("AppController2.rb")
      assert_match /ib_outlets :hogehoge/, File.read("AppController2.rb")
    end
  end

  def test_convertnib
    create
    cd 'Test Ruby Cocoa' do
      cp_r @testdir + 'Main.nib', '.'
      rubycocoa "convert", "Main.nib"
      res = Pathname.new 'AppController.rb'
      assert res.exist?, 'Create Converted .rb'

      time = res.mtime
      rubycocoa "convert", "Main.nib"
      assert_equal time, res.mtime, 'No overwrite existed .rb'

      assert File.exist?("ConfigController.rb")
    end
  end

  def test_convertheader
    create
    cd 'Test Ruby Cocoa' do
      cp_r @testdir + 'AppController.h', '.'
      rubycocoa "convert", "AppController.h"
      res = Pathname.new 'AppController.rb'
      assert res.exist?, 'Create Converted .rb'
    end
  end

  def test_updatenib
    create
    cd 'Test Ruby Cocoa' do
      cp_r @testdir + 'BulletsController.rb', '.'
      rubycocoa "update", "-a", "English.lproj/MainMenu.nib", "BulletsController.rb"
    end
  end

  def test_add
    create
    cd 'Test Ruby Cocoa' do
      cp_r @testdir + 'BulletsController.rb', '.'
      rubycocoa "add", "BulletsController.rb", "Test Ruby Cocoa.xcodeproj"
      #system(@rubycocoa, "add", "BulletsController.rb", "Test Ruby Cocoa.xcodeproj")
      assert_match /\*\* BUILD SUCCEEDED \*\*/, %x{xcodebuild}
      assert File.exist?("build/Release/Test Ruby Cocoa.app/Contents/Resources/BulletsController.rb")
    end
  end

  def test_raketasks
    create
    cd "Test Ruby Cocoa" do
      %x{rake create +a hello AppController}
      assert File.exists?("AppController.rb")

      %x{rake update +a AppController.rb}
      assert_match /hello/, File.read("English.lproj/MainMenu.nib/classes.nib")

      %x{rake package}
      assert File.exists?("build/Release/Test Ruby Cocoa.app/Contents/Resources/AppController.rb")
      assert File.exists?("pkg/Test Ruby Cocoa.#{Time.now.strftime("%Y-%m-%d")}.dmg")
    end
  end

  def create
    template = @testdir + "../../../../template/ProjectBuilder/Application/Cocoa-Ruby Application"
    rubycocoa "new", "--template", template, "Test Ruby Cocoa"
  end

  def rubycocoa(*args)
    stdout = $stdout
    $stdout = StringIO.new
    RubyCocoaCommand.run(args.map{|i| i.to_s })
    $stdout = stdout
  end

end
