# APPLE LOCAL vector instructions are not supported except with -faltivec

if { [istarget "*-apple-darwin*"] } { set options "-faltivec" }
return 0
