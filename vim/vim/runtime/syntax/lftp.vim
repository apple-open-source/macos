" Vim syntax file
" Language:	    lftp Configuration file
" Maintainer:	    Nikolai 'pcp' Weibull <da.box@home.se>
" URL:		    http://www.pcppopper.org/
" Latest Revision:  2002-10-24

if version < 600
    syntax clear
elseif exists("b:current_syntax")
    finish
endif

" Set iskeyword since we need `-' (and potentially others) in keywords.
" For version 5.x: Set it globally
" For version 6.x: Set it locally
if version >= 600
  command -nargs=1 SetIsk setlocal iskeyword=<args>
else
  command -nargs=1 SetIsk set iskeyword=<args>
endif
SetIsk 48-57,97-122,-
delcommand SetIsk

" comments
syn region  lftpComment		display oneline matchgroup=lftpComment start="#" end="$" contains=lftpTodo

" todo
syn keyword lftpTodo		contained TODO FIXME

" strings
syn region  lftpString		contained display oneline start=+"+ skip=+\\"+ end=+"+

" numbers
syn match   lftpNumber		contained display "\<\d\+\(\.\d\+\)\=\>"

" booleans and other things
syn keyword lftpBoolean		contained yes no on off true false

" intervals
syn keyword lftpInterval	contained infinity inf never forever
syn match   lftpInterval	contained "\<\(\d\+\(\.\d\+\)\=[dhms]\)\+\>"

" commands
syn keyword lftpKeywords	alias anon at bookmark cache cat cd chmod close
syn keyword lftpKeywords	command debug echo exit find get glob help jobs
syn keyword lftpKeywords	kill lcd lpwd ls mget fg mirror more mput mrm
syn keyword lftpKeywords	mv nlist open pget put pwd queue quote reget less
syn keyword lftpKeywords	rels renlist repeat reput rm rmdir scache site
syn keyword lftpKeywords	sleep source suspend user version wait zcat zmore

" settings
syn region  lftpSet		matchgroup=lftpKeywords start="set" end=";" end="$" contains=lftpString,lftpNumber,lftpBoolean,lftpInterval,lftpSettings

" not quite right since options can be given like save-pa or bmk:save
" but hopefully people type out the whole option when writing rc's
syn match   lftpSettings	contained "\(\<bmk:\)\=\<save-passwords\>"
syn match   lftpSettings	contained "\(\<cmd:\)\=\<\(at-exit\|csh-history\|default-protocol\|default-title\|fail-exit\|interactive\|long-running\|ls-default\)\>"
syn match   lftpSettings	contained "\(\<cmd:\)\=\<\(move-background\|prompt\|remote-completion\|save-cwd-history\|\(set-\)\=term-status\|verbose\|verify-\(host\|path\)\)\>"
syn match   lftpSettings	contained "\(\<dns:\)\=\<\(SRV-query\|cache-\(enable\|expire\|size\)\|fatal-timeout\|order\|use-fork\)\>"
syn match   lftpSettings	contained "\(\<ftp:\)\=\<\(acct\|anon-\(pass\|user\)\|auto-sync-mode\|bind-data-socket\|fix-pasv-address\)\>"
syn match   lftpSettings	contained "\(\<ftp:\)\=\<\(home\|list-options\|nop-interval\|passive-mode\|port-\(ipv4\|range\)\|proxy\)\>"
syn match   lftpSettings	contained "\(\<ftp:\)\=\<\(rest-\(list\|stor\)\|retry-530\(-anonymous\)\=\|site-group\|skey-\(allow\|force\)\)\>"
syn match   lftpSettings	contained "\(\<ftp:\)\=\<\(ssl-\(allow\|force\|protect-data\)\|stat-interval\|sync-mode\|fxp-passive-source\)\>"
syn match   lftpSettings	contained "\(\<ftp:\)\=\<\(use-\(abor\|fxp\|site-idle\|stat\|quit\)\|verify-\(address\|port\)\|web-mode\)\>"
syn match   lftpSettings	contained "\(\<hftp:\)\=\<\(cache\|proxy\|use-\(authorization\|head\|type\)\)\>"
syn match   lftpSettings	contained "\(\<http:\)\=\<\(accept\(-charset\|-language\)\=\|cache\|cookie\|post-content-type\|proxy\)\>"
syn match   lftpSettings	contained "\(\<http:\)\=\<\(put-\(method\|content-type\)\|referer\|set-cookies\|user-agent\)\>"
syn match   lftpSettings	contained "\(\<https:\)\=\<proxy\>"
syn match   lftpSettings	contained "\(\<mirror:\)\=\<\(loose-\)\=time-precision\>"
syn match   lftpSettings	contained "\(\<module:\)\=\<path\>"
syn match   lftpSettings	contained "\(\<net:\)\=\<\(connection-\(limit\|takeover\)\|idle\|limit-\(total-\)\=\(rate\|max\)\)\>"
syn match   lftpSettings	contained "\(\<net:\)\=\<\(max-retries\|no-proxy\|persist-retries\|reconnect-interval-\(base\|max\|multiplier\)\)\>"
syn match   lftpSettings	contained "\(\<net:\)\=\<\(socket-\(buffer\|maxseg\)\|timeout\)\>"
syn match   lftpSettings	contained "\(\<xfer:\)\=\<\(clobber\|eta-\(period\|terse\)\|max-redirections\|rate-period\|make-backup\)\>"

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_lftp_syn_inits")
    if version < 508
	let did_lftp_syn_inits = 1
	command -nargs=+ HiLink hi link <args>
    else
	command -nargs=+ HiLink hi def link <args>
    endif

    HiLink lftpComment	    Comment
    HiLink lftpTodo	    Todo
    HiLink lftpString	    String
    HiLink lftpNumber	    Number
    HiLink lftpBoolean	    Number
    HiLink lftpInterval	    Number
    HiLink lftpKeywords	    Keyword
    HiLink lftpSettings	    Type
    delcommand HiLink
endif

let b:current_syntax = "lftp"

" vim: set sw=4 sts=4:

