" Vim plugin with helper function(s) for --remote-wait
" Maintainer: Flemming Madsen <fma@cci.dk>
" Last Change: 2002 Feb 26

" Has this already been loaded?
if exists("loaded_rrhelper")
  finish
endif
let loaded_rrhelper = 1

" Setup answers for a --remote-wait client who will assume
" a SetupRemoteReplies() function in the command server

if has("clientserver")
  function SetupRemoteReplies()
    let cnt = 0
    let max = argc()

    let id = expand("<client>")
    if id == 0
      return
    endif
    while cnt < max
      " Handle same file from more clients and file being more than once
      " on the command line by encoding this stuff in the group name
      let uniqueGroup = "RemoteReply_".id."_".cnt
      execute "augroup ".uniqueGroup
      execute 'autocmd '.uniqueGroup.' BufUnload '.argv(cnt).'  call DoRemoteReply("'.id.'", "'.cnt.'", "'.uniqueGroup.'", "'.argv(cnt).'")'
      let cnt = cnt + 1
    endwhile
    augroup END
  endfunc

  function DoRemoteReply(id, cnt, group, file)
    call server2client(a:id, a:cnt)
    execute 'autocmd! '.a:group.' BufUnload '.a:file
    execute 'augroup! '.a:group
  endfunc

endif


" vim: set sw=2 sts=2 :
