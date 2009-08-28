#
# atol for long return check
#
package provide Atol 0.1
package require Ffidl
package require Ffidlrt

#
# the plain interfaces
#
ffidl-proc atol {pointer-utf8} long [ffidl-symbol [ffidl-find-lib c] atol]
ffidl-proc _strtol {pointer-utf8 pointer-var int} long [ffidl-symbol [ffidl-find-lib c] strtol]
ffidl-proc _strtoul {pointer-utf8 pointer-var int} {unsigned long} [ffidl-symbol [ffidl-find-lib c] strtoul]
#
# some cooked interfaces
#
proc strtol {str radix} {
    set endptr [binary format [ffidl-info format pointer] 0]
    set l [_strtol $str endptr $radix]
    binary scan $endptr [ffidl-info format pointer] endptr
    list $l $endptr
}
proc strtoul {str radix} {
    set endptr [binary format [ffidl-info format pointer] 0]
    set l [_strtoul $str endptr $radix]
    binary scan $endptr [ffidl-info format pointer] endptr
    list $l $endptr
}

