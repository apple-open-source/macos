" Vim filetype plugin file
" Language:	Perl
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Change:  2003 May 02
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

setlocal formatoptions+=crq

setlocal comments=:#
setlocal commentstring=#%s

" Make sure the continuation lines below do not cause problems in
" compatibility mode.
set cpo-=C

" Change the browse dialog on Win32 to show mainly Perl-related files
if has("gui_win32") && !exists("b:browsefilter")
    let b:browsefilter = "Perl Source Files (*.pl)\t*.pl\n" .
		       \ "Perl Modules (*.pm)\t*.pm\n" .
		       \ "Perl Documentation Files (*.pod)\t*.pod\n" .
		       \ "All Files (*.*)\t*.*\n"
endif

" Provided by Ned Konz <ned@bike-nomad.com>
"---------------------------------------------
setlocal include=\\<\\(use\|require\\)\\>
setlocal includeexpr=substitute(substitute(v:fname,'::','/','g'),'$','.pm','')
setlocal iskeyword=48-57,_,A-Z,a-z,:
setlocal define=[^A-Za-z_]

" Set this once, globally.
if !exists("perlpath")
    if executable("perl")
	if &shellxquote != '"'
	    let perlpath = system('perl -e "print join(q/,/,@INC)"')
	else
	    let perlpath = system("perl -e 'print join(q/,/,@INC)'")
	endif
	let perlpath = substitute(perlpath,',.$',',,','')
    else
	" If we can't call perl to get its path, just default to using the
	" current directory and the directory of the current file.
	let perlpath = ".,,"
    endif
endif

let &l:path=perlpath
