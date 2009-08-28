#
# getrusage for processor times
#
package provide Getrusage 0.1
package require Ffidl
package require Ffidlrt

#
# system types
#
::ffidl::find-type time_t
::ffidl::find-type clock_t
::ffidl::find-type timeval

#
# typedefs, avoid redefinition error
#
catch {
    ::ffidl::typedef rusage timeval timeval long long long long long long long long long long long long long long
}

#
# raw interfaces
#
::ffidl::callout _getrusage {int pointer-var} int [ffidl::symbol [ffidl::find-lib c] getrusage]
::ffidl::callout _clock {} clock_t [ffidl::symbol [ffidl::find-lib c] clock]

#
# the cooked interfaces
#
proc getrusage {who} {
    set rusage [binary format x[ffidl::info sizeof rusage]]
    switch $who {
	SELF { set rwho 0 }
	CHILDREN { set rwho -1 }
	BOTH { set rwho -2 }
    }
    if {[_getrusage $rwho rusage] != 0} {
	return {}
    }
    binary scan $rusage [ffidl::info format rusage] \
	v(ru_utime.tv_sec) v(ru_utime.tv_usec) \
	v(ru_stime.tv_sec) v(ru_stime.tv_usec) \
	v(ru_maxrss) v(ru_ixrss) v(ru_idrss) v(ru_isrss) \
	v(ru_minflt) v(ru_majflt) v(ru_nswap) \
	v(ru_inblock) v(ru_oublock) \
	v(ru_msgsnd) v(ru_msgrcv) \
	v(ru_nsignals) v(ru_nvcsw) v(ru_nivcsw)
    array get v
}
proc utime {} {
    array set v [getrusage SELF]
    expr {$v(ru_utime.tv_sec)+$v(ru_utime.tv_usec)*1.0e-6}
}
proc stime {} {
    array set v [getrusage SELF]
    expr {$v(ru_stime.tv_sec)+$v(ru_stime.tv_usec)*1.0e-6}
}
