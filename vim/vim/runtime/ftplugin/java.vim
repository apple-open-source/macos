" Vim filetype plugin file
" Language:	Java
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Change:  2003 May 02
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

" Only do this when not done yet for this buffer
if exists("b:did_ftplugin")
  finish
endif

" Don't load another plugin for this buffer
let b:did_ftplugin = 1

" Go ahead and set this to get decent indenting even if the indent files
" aren't being used.
setlocal cindent

"---------------------
" From Johannes Zellner <johannes@zellner.org>
setlocal cinoptions+=j1		" Correctly indent anonymous classes
"---------------------

" For filename completion, prefer the .java extension over the .class
" extension.
set suffixes+=.class

" Automatically add the java extension when searching for files, like with gf
" or [i
setlocal suffixesadd=.java

" Set 'formatoptions' to break comment lines but not other lines,
" and insert the comment leader when hitting <CR> or using "o".
setlocal formatoptions-=t formatoptions+=croql

" Set 'comments' to format dashed lists in comments
setlocal comments& comments^=sO:*\ -,mO:*\ \ ,exO:*/  " Behaves just like C

setlocal commentstring=//%s
" Make sure the continuation lines below do not cause problems in
" compatibility mode.
set cpo-=C

" Change the :browse e filter to primarily show Java-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="Java Files (*.java)\t*.java\n" .
		\	"Properties Files (*.prop*)\t*.prop*\n" .
		\	"Manifest Files (*.mf)\t*.mf\n" .
		\	"All Files (*.*)\t*.*\n"
endif
