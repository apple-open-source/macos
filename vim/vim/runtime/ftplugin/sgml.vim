" Vim filetype plugin file
" Language:	sgml
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/xml.vim ftplugin/xml_*.vim ftplugin/xml/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show xml-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="SGML Files (*.sgml)\t*.sgml\n" . b:browsefilter
endif
