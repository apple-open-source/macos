" Vim filetype plugin file
" Language:	C#
" Maintainer:	Johannes Zellner <johannes@zellner.org>
" Last Change:	Sat, 24 May 2003 11:53:33 CEST

" Only do this when not done yet for this buffer
if exists("b:did_ftplugin")
  finish
endif

" Don't load another plugin for this buffer
let b:did_ftplugin = 1

setlocal cindent

" Set 'formatoptions' to break comment lines but not other lines,
" and insert the comment leader when hitting <CR> or using "o".
setlocal fo-=t fo+=croql

" Set 'comments' to format dashed lists in comments.
setlocal comments=sO:*\ -,mO:*\ \ ,exO:*/,s1:/*,mb:*,ex:*/,://

if has("gui_win32") && !exists("b:browsefilter")
    let b:browsefilter = "C# Source Files (*.cs)\t*.cs\n" .
		       \ "All Files (*.*)\t*.*\n"
endif
