#
# qsort for callback testing
#
#        void qsort(void *base, size_t nmemb, size_t size,
#              int (*compar)(const void *, const void *))
#

package provide Qsort 0.1
package require Ffidl
package require Ffidlrt

#
# test for presence of ::ffidl::callback
#
if {[llength [info commands ::ffidl::callback]] == 0} {
    return
}

#
# system types
#
::ffidl::find-type size_t

#
# interface
#
::ffidl::callout qsort {pointer-var size_t size_t pointer-proc} void [::ffidl::symbol [::ffidl::find-lib c] qsort]

