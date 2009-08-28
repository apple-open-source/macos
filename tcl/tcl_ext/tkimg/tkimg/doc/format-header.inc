[comment {-*- tcl -*- doctools = format-header.inc}]

[manpage_begin img-[vset FORMATNAME] n 1.3]
[moddesc Img]
[titledesc "Img, [vset FORMATFULLNAME] ([vset FORMATNAME])"]
[copyright {1995-2003 Jan Nijtmans <nijtmans@users.sourceforge.net>}]
[require Tk]
[require img::[vset FORMATNAME] [opt 1.3]]
[description]

The package [package img::[vset FORMATNAME]] is a sub-package of
[package Img]. It can be loaded as a part of the complete Img support,
via [cmd {package require Img}], or on its own, via

[cmd "package require img::[vset FORMATNAME]"].

[para]

Like all packages of [package Img] it does [emph not] provide new
commands, but extends the existing [package Tk] command [cmd image] so
that it supports files containing raster images in the

[vset FORMATFULLNAME] ([vset FORMATNAME]). More specifically

[package img::[vset FORMATNAME]] extends [package Tk]'s [cmd photo]
image type.

[para]

The name of the new format handler is [const [vset FORMATNAME]].
