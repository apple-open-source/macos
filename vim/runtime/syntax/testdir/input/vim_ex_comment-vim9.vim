vim9script

# Vim comments

# Vim9-script comment

 # string only recognised with leading char
 "useless string"

function! Foo()
  " Legacy-script comment
  # 42 " comment
endfunction

def! Bar()
  # Vim9-script comment
  "useless string" # comment
enddef

command -count FooCommand {
  # Vim9-script comment
  "useless string" # comment
}

autocmd BufNewFile * {
  # Vim9-script comment
  "useless string" # comment
}


# Multiline comments

# comment
  \ continuing comment
  \ continuing comment

# :Foo
      \ arg1
      #\ comment
      \ arg2

echo "TOP"


# Line-continuation comments

:Foo
      #\ line continuation comment
      \ arg1
      #\ line continuation comment
      \ arg2


# Issue: #13047

if !exists(":DiffOrig")
  command DiffOrig vert new | set bt=nofile | r ++edit %% | 0d_ | diffthis
		  \ | wincmd p | diffthis
endif


# Issue: #11307 and #11560

# This is what we call " blah
