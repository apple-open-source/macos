" Vim filetype plugin file
" Language:	tcsh
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/csh.vim ftplugin/csh_*.vim ftplugin/csh/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show tcsh-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="tcsh Scripts (*.tcsh)\t*.tcsh\n" . b:browsefilter
endif
