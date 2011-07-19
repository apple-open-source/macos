
Welcome to [package diagram], a package for the easy construction of
diagrams (sic), i.e. 2D vector graphics, sometimes also called [term \
pictures].

Note that this package is not a replacement for [package Tk]'s canvas,
but rather a layer sitting on top of it, to make it easier to use.

In other words, using the canvas as the core graphics engine [package \
diagram] abstracts away from the minutiae of handling coordinates to
position and size the drawn elements, allowing the user to concentrate
on the content of the diagram instead.

[para]

This is similar to Brian Kernighan's PIC language for troff, which is
the spiritual ancestor of this package.

[para]

This document contains the reference to the API and drawing (language)
commands. Its intended audience are users of the package wishing to
refresh their memory.

Newcomers should read the [term {Diagram Language Tutorial}] first.

Developers wishing to work on the internals of the package and its
supporting packages should look at section
	[sectref {Diagram Classes}]
first, and then the comments in the sources of the packages itself.

[para]

In the remainder of the document we first describe the APIs of the
diagram class and its instances, followed by the language reference
for the drawing language itself.
