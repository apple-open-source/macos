" Vim syntax file
" Language:	Slrn setup file (based on slrn 0.9.7.4)
" Maintainer:	Preben 'Peppe' Guldberg <peppe-vim@wielders.org>
" Last Change:	30 May 2003

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn keyword slrnrcTodo		contained Todo

" In some places whitespace is illegal
syn match slrnrcSpaceError	contained "\s"

syn match slrnrcNumber		contained "-\=\<\d\+\>"
syn match slrnrcNumber		contained +'[^']\+'+

syn match slrnrcSpecKey		contained +\(\\[er"']\|\^[^'"]\|\\\o\o\o\)+

syn match  slrnrcKey		contained "\S\+"	contains=slrnrcSpecKey
syn region slrnrcKey		contained start=+"+ skip=+\\"+ end=+"+ oneline contains=slrnrcSpecKey
syn region slrnrcKey		contained start=+'+ skip=+\\'+ end=+'+ oneline contains=slrnrcSpecKey

syn match slrnrcSpecChar	contained +'+
syn match slrnrcSpecChar	contained +\\[n"]+
syn match slrnrcSpecChar	contained "%[dfmnrs%]"

syn match  slrnrcString		contained /[^ \t%"']\+/	contains=slrnrcSpecChar
syn region slrnrcString		contained start=+"+ skip=+\\"+ end=+"+ oneline contains=slrnrcSpecChar

syn match slrnSlangPreCondit	"^#\s*ifn\=\(def\>\|false\>\|true\>\|\$\)"
syn match slrnSlangPreCondit	"^#\s*\(elif\|else\|endif\)\>"

syn match slrnrcComment		"%.*$"	contains=slrnrcTodo

syn region  slrnrcVarIntStr	contained abort_unmodified_edits article_window_page_overlap auto_mark_article_as_read beep broken_xref cc_followup check_new_groups color_by_score confirm_actions custom_sort_by_threads
syn region  slrnrcVarIntStr	contained display_cursor_bar drop_bogus_groups editor_uses_mime_charset emphasized_text_mask emphasized_text_mode fold_headers followup_strip_signature force_authentication generate_date_header generate_email_from
syn region  slrnrcVarIntStr	contained generate_message_id grouplens_port hide_pgpsignature hide_quotes hide_signature hide_verbatim_marks highlight_unread_subjects highlight_urls ignore_signature kill_score
syn region  slrnrcVarIntStr	contained lines_per_update mail_editor_is_mua max_low_score max_queued_groups min_high_score mouse netiquette_warnings new_subject_breaks_threads no_autosave no_backups
syn region  slrnrcVarIntStr	contained prefer_head process_verbatim_marks query_next_article query_next_group query_read_group_cutoff read_active reject_long_lines scroll_by_page show_article show_thread_subject
syn region  slrnrcVarIntStr	contained simulate_graphic_chars smart_quote sorting_method spoiler_char spoiler_display_mode spool_check_up_on_nov uncollapse_threads unsubscribe_new_groups use_blink use_color
syn region  slrnrcVarIntStr	contained use_flow_control use_grouplens use_header_numbers use_inews use_localtime use_metamail use_mime use_slrnpull use_tilde use_tmpdir
syn region  slrnrcVarIntStr	contained use_uudeview warn_followup_to wrap_flags wrap_method write_newsrc_flags

" Listed for removal
syn region  slrnrcVarIntStr	contained author_display display_author_realname display_score group_dsc_start_column process_verbatum_marks prompt_next_group query_reconnect show_descriptions use_xgtitle

" Match as a "string" too
syn region  slrnrcVarIntStr	contained matchgroup=slrnrcVarInt start=+"+ end=+"+ oneline contains=slrnrcVarInt,slrnrcSpaceError

syn keyword slrnrcVarStr	contained Xbrowser art_help_line art_status_line cc_followup_string cc_post_string charset custom_headers custom_sort_order decode_directory editor_command
syn keyword slrnrcVarStr	contained failed_posts_file followup_custom_headers followup_date_format followup_string group_help_line group_status_line grouplens_host grouplens_pseudoname header_help_line header_status_line
syn keyword slrnrcVarStr	contained hostname inews_program macro_directory mail_editor_command metamail_command mime_charset non_Xbrowser organization overview_date_format post_editor_command
syn keyword slrnrcVarStr	contained post_object postpone_directory printer_name quote_string realname reply_custom_headers reply_string replyto save_directory save_posts
syn keyword slrnrcVarStr	contained save_replies score_editor_command scorefile sendmail_command server_object signature signoff_string spool_active_file spool_activetimes_file spool_inn_root
syn keyword slrnrcVarStr	contained spool_newsgroups_file spool_nov_file spool_nov_root spool_root supersedes_custom_headers top_status_line username

" Listed for removal
syn keyword slrnrcVarStr	contained followup

" Match as a "string" too
syn region  slrnrcVarStrStr	contained matchgroup=slrnrcVarStr start=+"+ end=+"+ oneline contains=slrnrcVarStr,slrnrcSpaceError

" Various commands
syn region slrnrcCmdLine	matchgroup=slrnrcCmd start="\<\(autobaud\|color\|compatible_charsets\|group_display_format\|grouplens_add\|header_display_format\|ignore_quotes\|include\|interpret\|mono\|nnrpaccess\|posting_host\|server\|set\|setkey\|strip_re_regexp\|strip_sig_regexp\|strip_was_regexp\|unsetkey\|visible_headers\)\>" end="$" oneline contains=slrnrc\(String\|Comment\)

" Listed for removal
syn region slrnrcCmdLine	matchgroup=slrnrcCmd start="\(cc_followup_string\|decode_directory\|editor_command\|followup\|hostname\|organization\|quote_string\|realname\|replyto\|scorefile\|signature\|username\)" end="$" oneline contains=slrnrc\(String\|Comment\)

" Setting variables
syn keyword slrnrcSet		contained set
syn match   slrnrcSetStr	"^\s*set\s\+\S\+" skipwhite nextgroup=slrnrcString contains=slrnrcSet,slrnrcVarStr\(Str\)\=
syn match   slrnrcSetInt	contained "^\s*set\s\+\S\+" contains=slrnrcSet,slrnrcVarInt\(Str\)\=
syn match   slrnrcSetIntLine	"^\s*set\s\+\S\+\s\+\(-\=\d\+\>\|'[^']\+'\)" contains=slrnrcSetInt,slrnrcNumber,slrnrcVarInt

" Color definitions
syn match   slrnrcColorObj	contained "\<quotes\d\+\>"
syn keyword slrnrcColorObj	contained article author boldtext box cursor date description error frame from_myself
syn keyword slrnrcColorObj	contained group grouplens_display header_name header_number headers high_score italicstext menu menu_press message
syn keyword slrnrcColorObj	contained neg_score normal pgpsignature pos_score quotes response_char selection signature status subject
syn keyword slrnrcColorObj	contained thread_number tilde tree underlinetext unread_subject url verbatim

" Listed for removal
syn keyword slrnrcColorObj	contained verbatum

syn region  slrnrcColorObjStr	contained matchgroup=slrnrcColorObj start=+"+ end=+"+ oneline contains=slrnrcColorObj,slrnrcSpaceError
syn keyword slrnrcColorVal	contained default
syn keyword slrnrcColorVal	contained black blue brightblue brightcyan brightgreen brightmagenta brightred brown cyan gray
syn keyword slrnrcColorVal	contained green lightgray magenta red white yellow
syn region  slrnrcColorValStr	contained matchgroup=slrnrcColorVal start=+"+ end=+"+ oneline contains=slrnrcColorVal,slrnrcSpaceError
" Mathcing a function with three arguments
syn keyword slrnrcColor		contained color
syn match   slrnrcColorInit	contained "^\s*color\s\+\S\+" skipwhite nextgroup=slrnrcColorVal\(Str\)\= contains=slrnrcColor\(Obj\|ObjStr\)\=
syn match   slrnrcColorLine	"^\s*color\s\+\S\+\s\+\S\+" skipwhite nextgroup=slrnrcColorVal\(Str\)\= contains=slrnrcColor\(Init\|Val\|ValStr\)

" Mono settings
syn keyword slrnrcMonoVal	contained blink bold none reverse underline
syn region  slrnrcMonoValStr	contained matchgroup=slrnrcMonoVal start=+"+ end=+"+ oneline contains=slrnrcMonoVal,slrnrcSpaceError
" Color object is inherited
" Mono needs at least one argument
syn keyword slrnrcMono		contained mono
syn match   slrnrcMonoInit	contained "^\s*mono\s\+\S\+" contains=slrnrcMono,slrnrcColorObj\(Str\)\=
syn match   slrnrcMonoLine	"^\s*mono\s\+\S\+\s\+\S.*" contains=slrnrcMono\(Init\|Val\|ValStr\),slrnrcComment

" Functions in article mode
syn keyword slrnrcFunArt    contained article_bob article_eob article_left article_line_down article_line_up article_page_down article_page_up article_right article_search author_search_backward
syn keyword slrnrcFunArt    contained author_search_forward browse_url cancel catchup catchup_all create_score decode delete delete_thread digit_arg
syn keyword slrnrcFunArt    contained enlarge_article_window evaluate_cmd exchange_mark expunge fast_quit followup forward forward_digest get_children_headers get_parent_header
syn keyword slrnrcFunArt    contained grouplens_rate_article goto_article goto_last_read header_bob header_eob header_line_down header_line_up header_page_down header_page_up help
syn keyword slrnrcFunArt    contained hide_article locate_article mark_spot next next_high_score next_same_subject pipe post post_postponed previous
syn keyword slrnrcFunArt    contained print quit redraw repeat_last_key reply save show_spoilers shrink_article_window skip_quotes skip_to_next_group
syn keyword slrnrcFunArt    contained skip_to_previous_group subject_search_backward subject_search_forward supersede suspend tag_header toggle_collapse_threads toggle_header_formats toggle_header_tag toggle_headers
syn keyword slrnrcFunArt    contained toggle_pgpsignature toggle_quotes toggle_rot13 toggle_signature toggle_verbatim_marks uncatchup uncatchup_all undelete untag_headers wrap_article
syn keyword slrnrcFunArt    contained zoom_article_window view_scores toggle_sort

" Listed for removal
syn keyword slrnrcFunArt    contained art_bob art_eob art_xpunge article_linedn article_lineup article_pagedn article_pageup down enlarge_window goto_beginning
syn keyword slrnrcFunArt    contained goto_end left locate_header_by_msgid pagedn pageup pipe_article prev print_article right scroll_dn
syn keyword slrnrcFunArt    contained scroll_up shrink_window skip_to_prev_group toggle_show_author up

" Functions in group mode

syn keyword slrnrcFunGroup  contained add_group bob catchup digit_arg eob evaluate_cmd group_search group_search_backward group_search_forward help
syn keyword slrnrcFunGroup  contained line_down line_up move_group page_down page_up post post_postponed quit redraw refresh_groups
syn keyword slrnrcFunGroup  contained repeat_last_key save_newsrc select_group subscribe suspend toggle_group_formats toggle_hidden toggle_list_all toggle_scoring transpose_groups
syn keyword slrnrcFunGroup  contained uncatchup unsubscribe

" Listed for removal
syn keyword slrnrcFunGroup	contained down group_bob group_eob pagedown pageup toggle_group_display uncatch_up up

" Functions in readline mode (actually from slang's slrline.c)
syn keyword slrnrcFunRead	contained bdel bol del deleol down enter eol left quoted_insert right trim up

" Binding keys
syn keyword slrnrcSetkeyObj	contained article group readline
syn region  slrnrcSetkeyObjStr	contained matchgroup=slrnrcSetkeyObj start=+"+ end=+"+ oneline contains=slrnrcSetkeyObj
syn match   slrnrcSetkeyArt	contained '\("\=\)\<article\>\1\s\+\S\+' skipwhite nextgroup=slrnrcKey contains=slrnrcSetKeyObj\(Str\)\=,slrnrcFunArt
syn match   slrnrcSetkeyGroup	contained '\("\=\)\<group\>\1\s\+\S\+' skipwhite nextgroup=slrnrcKey contains=slrnrcSetKeyObj\(Str\)\=,slrnrcFunGroup
syn match   slrnrcSetkeyRead	contained '\("\=\)\<readline\>\1\s\+\S\+' skipwhite nextgroup=slrnrcKey contains=slrnrcSetKeyObj\(Str\)\=,slrnrcFunRead
syn match   slrnrcSetkey	"^\s*setkey\>" skipwhite nextgroup=slrnrcSetkeyArt,slrnrcSetkeyGroup,slrnrcSetkeyRead

" Unbinding keys
syn match   slrnrcUnsetkey	'^\s*unsetkey\s\+\("\)\=\(article\|group\|readline\)\>\1' skipwhite nextgroup=slrnrcKey contains=slrnrcSetkeyObj\(Str\)\=

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_slrnrc_syntax_inits")
  if version < 508
    let did_slrnrc_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink slrnrcTodo		Todo
  HiLink slrnrcSpaceError	Error
  HiLink slrnrcNumber		Number
  HiLink slrnrcSpecKey		SpecialChar
  HiLink slrnrcKey		String
  HiLink slrnrcSpecChar		SpecialChar
  HiLink slrnrcString		String
  HiLink slrnSlangPreCondit	Special
  HiLink slrnrcComment		Comment
  HiLink slrnrcVarInt		Identifier
  HiLink slrnrcVarStr		Identifier
  HiLink slrnrcCmd		slrnrcSet
  HiLink slrnrcSet		Operator
  HiLink slrnrcColor		Keyword
  HiLink slrnrcColorObj		Identifier
  HiLink slrnrcColorVal		String
  HiLink slrnrcMono		Keyword
  HiLink slrnrcMonoObj		Identifier
  HiLink slrnrcMonoVal		String
  HiLink slrnrcFunArt		Macro
  HiLink slrnrcFunGroup		Macro
  HiLink slrnrcFunRead		Macro
  HiLink slrnrcSetkeyObj	Identifier
  HiLink slrnrcSetkey		Keyword
  HiLink slrnrcUnsetkey		slrnrcSetkey

  delcommand HiLink
endif

let b:current_syntax = "slrnrc"

"EOF	vim: ts=8 noet tw=120 sw=8 sts=0
