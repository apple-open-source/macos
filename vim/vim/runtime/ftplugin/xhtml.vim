" Vim filetype plugin file
" Language:	xhtml
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Dec 04
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/html.vim ftplugin/html_*.vim ftplugin/html/*.vim
unlet b:did_ftplugin
runtime! ftplugin/xml.vim ftplugin/xml_*.vim ftplugin/xml/*.vim

let b:did_ftplugin = 1
