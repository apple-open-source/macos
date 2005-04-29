# APPLE LOCAl  vector instructions are not supported except with -faltivec. xfailed it. 

if { [istarget "*-apple-darwin*"] } {
      set torture_compile_xfail "*-apple-darwin*"  
}

return 0
