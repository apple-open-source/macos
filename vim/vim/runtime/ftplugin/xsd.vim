" Vim filetype plugin file
" Language:	xsd
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/xml.vim ftplugin/xml_*.vim ftplugin/xml/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show xsd-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="XSD Files (*.xsd)\t*.xsd\n" . b:browsefilter
endif
