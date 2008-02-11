#!/usr/bin/env ruby
#
# pdf2tiff.rb
#
# Convert a PDF document into a series of TIFF images
#
# Created by Ernest Prabhakar. Copyright 2007 Apple, Inc. All Rights Reserved
#

require "osx/cocoa"
include OSX
OSX.require_framework 'PDFKit'

puts "Usage: #{__FILE__} [file1.pdf] [file2.pdf] ..." unless ARGV.size > 1

OSX::NSApplication.sharedApplication

ARGV.each do |path|
  url = NSURL.fileURLWithPath_ path
  file = path.split("/")[-1]
  root = file.split(".")[0]

  pdfdoc = PDFDocument.alloc.initWithURL_ url

  pdfdoc.pageCount.times do |i|
    page = pdfdoc.pageAtIndex_ i
    pdfdata = page.dataRepresentation
    image = NSImage.alloc.initWithData_ pdfdata
    tiffdata = image.TIFFRepresentation
    outfile = "#{root}_#{i}.tiff"
    puts "Writing #{page.description} to #{outfile} for #{path}"
    tiffdata.writeToFile_atomically_(outfile,false)
  end
end
