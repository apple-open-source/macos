" Vim filetype plugin file
" Language:	jsp
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Dec 04
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/html.vim ftplugin/html_*.vim ftplugin/html/*.vim
unlet b:did_ftplugin
runtime! ftplugin/java.vim ftplugin/java_*.vim ftplugin/java/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show JSP-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="JSP Files (*.jsp)\t*.jsp\n" . b:browsefilter
endif
