" Vim filetype plugin file
" Language:	sh
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2003 Apr 16
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

setlocal commentstring=#%s

" Shell:  thanks to Johannes Zellner
if exists("loaded_matchit") && !exists("b:match_words")
    let s:sol = '\%(;\s*\|^\s*\)\@<='  " start of line
    let b:match_words =
    \ s:sol.'if\>:' . s:sol.'elif\>:' . s:sol.'else\>:' . s:sol. 'fi\>,' .
    \ s:sol.'\%(for\|while\)\>:' . s:sol. 'done\>,' .
    \ s:sol.'case\>:' . s:sol. 'esac\>'
endif

" Change the :browse e filter to primarily show shell-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="Bourne Shell Scripts (*.sh)\t*.sh\n" .
		\	"Korn Shell Scripts (*.ksh)\t*.ksh\n" .
		\	"Bash Shell Scripts (*.bash)\t*.bash\n" .
		\	"All Files (*.*)\t*.*\n"
endif
