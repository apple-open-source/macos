" Vim filetype plugin file
" Language:	pascal
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

if exists("loaded_matchit") && !exists("b:match_words")
    let b:match_words='\<begin\>:\<end\>'
endif
