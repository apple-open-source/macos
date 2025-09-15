# Breaking compatibility (without breaking compatibility)

As we import newer versions of Open Source components into Darwin, we
will sometimes encounter situations where the upstream maintainer has
made changes that break backward compatibility and we have no choice
but to follow suit.  This forces us to fork the component, in part or
in whole: we need to maintain binary compatibility with the previous
version while providing the new version to new consumers.

## Old and new code side by side

Let's imagine that Darwin includes an Open Source library named
Frobnitz, currently at version 2.7.  The upstream maintainers have
released a new version, Frobnitz 3.0, which is not backward compatible
with Frobnitz 2.7.  We will therefore have to keep a copy of Frobnitz
2.7 for backward compatibility, while providing Frobnitz 3.0 (and
newer, as time goes on) to applications that target newer releases.

Since Frobnitz also includes a header file, `frobnitz.h`, we will
rename Frobnitz 2.7's copy to `frobnitz2.h` and Frobnitz 3.0's copy to
`frobnitz3.h` (alternatively, we might keep the original names but
move the Frobnitz 2.7 header(s) into a `frobnitz2` subdirectory and
import the Frobnitz 3.0 headers into a `frobnitz3` subdirectory) and
create a new `frobnitz.h` which pulls in the right header based on
which version the consumer needs.

## Version selection

The first decision we need to make is the transition point.  We not
only need to keep a copy of the old code to allow existing binaries to
run on newer OS versions, we also need to make it possible to build
new binaries that target an existing OS version which does not have
the new code.  The transition point will be the first OS version to
get the new code.

In `frobnitz.h`, we need to select the old or new version based on the
following criteria:

* First, we allow the application to explicitly ask for either the new
  or the old version.  This will make it much easier to test both.

* Next, if a target SDK version was specified, we compare it to our
  transition point.  If the requested version is older than the
  transition point, we select the old version.

* Finally, if we did not encounter a condition that requires selecting
  the old version, we default to the new version.

Let's say our transition point for Frobnitz 3.0 is macOS 16 and iOS 19
(we will ignore other targets for the sake of simplicity).  The header
file will have to start by determining which version to select:

```
#include <Availability.h>
#if defined(WANT_FROBNITZ_3)
# undef WANT_FROBNITZ_2
#elif !defined(WANT_FROBNITZ_2)
# if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 160000
#  define WANT_FROBNITZ_2 1
# elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 190000
#  define WANT_FROBNITZ_2 1
# endif
#endif
```

Keep in mind that undefined symbols evaluate to 0 when used in a
context where a number is expected, so the following would not work:

```
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 160000
# error This is also true if __MAC_OS_X_VERSION_MIN_REQUIRED is undefined!
#endif
```

We could avoid repeatedly defining `WANT_FROBNITZ_2` by placing
all our conditions in a single `#if`, like so:

```
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 160000) || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 190000)
# define WANT_FROBNITZ_2 1
#endif
```

However, this quickly becomes unreadable as we add more targets, and
if we split the condition over several lines, the header becomes
impossible to process with `unifdef(1)`, which does not support line
continuations.

```
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 160000) \
 || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 190000)
# error unreadable by unifdef(1) - older versions will silently ignore, newer ones will error out
#endif
```

Now that we've selected a version of Frobnitz, we can include the real
header:

```
#if WANT_FROBNITZ_2
# include <frobnitz2.h>
#else
# include <frobnitz3.h>
#endif
```

Don't forget to define either `WANT_FROBNITZ_2` or `WANT_FROBNITZ_3`
at the top of every source file that makes up Frobnitz itself, so it
gets the right header.  Unit or regression tests that work with both
versions can be built twice, once with `-DWANT_FROBNITZ_2` in `CFLAGS`
and once with `-DWANT_FROBNITZ_3`.

## Symbol versioning

Since the Frobnitz 3.0 API has significant overlap with the Frobnitz
2.7 API, we need to be able to distinguish between old and new
versions of identically-named functions.  Note that we cannot rename
the old ones, as that would prevent running existing binaries on newer
Darwin releases; so Frobnitz 2.7 API symbols will keep their original
names, while Frobnitz 3.0 symbols will be renamed.

### The mechanics of symbol versioning in Darwin

Although lld supports version maps, Darwin has not used them in the
past, and switching would be a monumental endeavor.  Instead, we use
inline assembly directives to assign alternate names to C symbols.
For instance, the following declares a function which is named `foo()`
in C code but ends up being named `_bar` at the object level.

```
int foo(void) __asm("_bar");
```

We could achieve the same effect with preprocessor macros, but it
would be fragile.  Consider this example:

```
#define foo bar
int foo(void);
```

This will result in `foo()` being named `_bar` at the object level, as
we intend, but it will also cause, say, `struct foo` to be renamed to
`struct bar` (anywhere the `foo` macro is in scope), which was not
what we intended.  We could avoid this by using a function-like macro:

```
#define foo(...) bar(__VA_ARGS__)
int foo(void);
```

But this will not work if we ever need to take the address of the
function, because `&foo`, without parentheses, will not be translated
to `&bar`.  Nor will it work if the function call is parenthesized,
which is perfectly valid C:

```
int ret = (foo)(); // will call _foo, not _bar
```

### Picking our names

We will use a unique suffix to distinguish the new symbols from the
old.  To avoid accidental collisions, the suffix begins with a `$`,
which is allowed by the linker but not by the C compiler.  What we put
after the `$` is entirely up to us.

This mapping needs to be visible both when compiling Frobnitz itself
and when compiling consumers of Frobnitz, so we do it in the header.
For instance, to rename the `frobnitz_init()` function (which is known
as `_frobnitz_init` to the linker) to `_frobnitz_init$FZ3`, we could
modify the prototype provided for `frobnitz_init()` in `frobnitz3.h`
to the following:

```
int frobnitz_init(const char *) __asm("_frobnitz_init$FZ3");
```

This quickly gets unwieldy and error-prone, so we do something like
this instead:

```
#define FZ3(sym) __asm("_" #sym "$FZ3")

int frobnitz_init(const char *)
    FZ3(frobnitz_init);
```

Note that we need to apply this technique to all symbols with global
linkage, not just functions:

```
extern int frobnitz_debug_lvl
    FZ3(frobnitz_debug_lvl);
```

If we build our library and run `objdump -t` on it now, we should see
something like this:

```
0000000000038be0 g     F __TEXT,__text _frobnitz_init
000000000004e148 g     F __TEXT,__text _frobnitz_init$FZ3
00000000000921fc g     O __DATA,__common _frobnitz_debug_lvl
00000000000921f8 g     O __DATA,__common _frobnitz_debug_lvl$FZ3
```

## Partial shims

Keeping a complete copy of Frobnitz 2.7 is far from ideal.  What if we
could get away with just a small part of it, or even just shim the
functions that have changed?

Let's say, for instance, that the only _incompatible_ change between
Frobnitz 2.7 and 3.0 is that `frobnitz_init()` previously did not take
any arguments, while it now takes a path to a configuration file.  We
still need to version the symbol, but instead of keeping a copy of the
old library, we can simply provide a compatibility shim for
`frobnitz_init()`.

We start by adding the same version selection logic as before to the
top of `frobnitz.h`:

```
#include <Availability.h>
#if defined(WANT_FROBNITZ_3)
# undef WANT_FROBNITZ_2
#elif !defined(WANT_FROBNITZ_2)
# if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 160000
#  define WANT_FROBNITZ_2 1
# elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 190000
#  define WANT_FROBNITZ_2 1
# endif
#endif
```

Next, we define the `FZ3` versioning macro as before, and also a
convenience `FZ2` macro which we will use to name the shim:

```
#define FZ2(sym) __asm("_" #sym)
#define FZ3(sym) __asm("_" #sym "$FZ3")
```

Third, we need to modify the prototype for `frobnitz_init()`.
Applications that target old releases receive the unversioned
compatibility shim, while applications that target new releases
receive the actual `frobnitz_init()`, which is versioned:

```
#if defined(WANT_FROBNITZ_2)
int frobnitz_init(void) FZ2(frobnitz_init);
#else
int frobnitz_init(const char *) FZ3(frobnitz_init);
#endif
```

The use of the `FZ2()` macro in the legacy prototype is a no-op, since
the object-level name it specifies is exactly what the compiler would
have picked anyway, but we include it for symmetry and clarity.

Finally, we add the compatibility shim in a separate source file:

```
#include <stddef.h>
#define WANT_FROBNITZ_3
#include <frobnitz.h>
int frobnitz_init_shim(void) FZ2(frobnitz_init);
int frobnitz_init_shim(void)
{
	return frobnitz_init(NULL); /* use hardcoded defaults */
}
```

Here the `FZ2()` macro is essential: we cannot name our shim
`frobnitz_init()` since we need to have the actual prototype for the
new `frobnitz_init()` in scope, so we name it `frobnitz_init_shim()`
instead and use the `FZ2` macro to ensure it is mapped to the correct
symbol.
