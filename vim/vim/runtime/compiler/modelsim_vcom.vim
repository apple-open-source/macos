" Vim Compiler File
" Compiler:	Modelsim Vcom
" Maintainer:	Paul Baleme <pbaleme@mail.com>
" Last Change:	September 25, 2002

if exists("current_compiler")
  finish
endif
let current_compiler = "modelsim_vcom"

setlocal errorformat=%tRROR:\ %f(%l):\ %m,%tARNING\[%*[0-9]\]:\ %m

