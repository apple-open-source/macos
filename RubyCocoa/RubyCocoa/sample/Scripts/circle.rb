# Create a new PDF file and draw a red circle in it, using Core Graphics.

require 'osx/cocoa'
include OSX

path = "circle.pdf"
url = CFURLCreateFromFileSystemRepresentation(nil, path, nil)
rect = CGRect.new(CGPoint.new(0, 0), CGSize.new(612, 792)) # Landscape
pdf = CGPDFContextCreateWithURL(url, rect, nil)

CGPDFContextBeginPage(pdf, nil)
CGContextSetRGBFillColor(pdf, 1.0, 0.0, 0.0, 1.0)
CGContextAddArc(pdf, 300, 300, 100, 0, 2 * Math::PI, 1)
CGContextFillPath(pdf)
CGPDFContextEndPage(pdf)
CGContextFlush(pdf)
