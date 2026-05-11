// undef_foundation_macros.h — workaround for Foundation / AssertMacros.h conflicts
//
// Foundation / AssertMacros.h define check, require, and verify as macros.
// These conflict with identifiers in libsecurity_codesigning (e.g.
// DynamicHash::verify()). Import Foundation first, then include this header
// to undefine the problematic macros.

#undef check
#undef require
#undef verify
