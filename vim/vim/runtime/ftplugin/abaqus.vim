" Vim filetype plugin file
" Language:	Abaqus finite element input file (www.hks.com)
" Maintainer:	Carl Osterwisch <osterwischc@asme.org>
" Last Change:	2003 May 11

" Only do this when not done yet for this buffer
if exists("b:did_ftplugin")
  finish
endif

" Don't load another plugin for this buffer
let b:did_ftplugin = 1

" Folding
if version >= 600
  set foldexpr=getline(v:lnum)[0]!=\"\*\"
  set foldmethod=expr
endif

" Win32 can filter files in the browse dialog
if has("gui_win32") && !exists("b:browsefilter")
    let b:browsefilter = "Abaqus Input Files (*.inp *.inc)\t*.inp;*.inc\n" .
	\ "Abaqus Results (*.dat *.pre)\t*.dat;*.pre\n" .
	\ "All Files (*.*)\t*.*\n"
endif
