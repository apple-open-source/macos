# APPLE LOCAL file radar 4204303
# skip for -fomit-frame-pointer
set torture_eval_before_compile {
  if {[istarget "*-*-darwin*"] & [string match {*-fomit-frame-pointer *} "$option"]} {
    continue
  }
}
return 0
