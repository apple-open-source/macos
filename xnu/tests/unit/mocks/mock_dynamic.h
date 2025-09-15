/*
 * Copyright (c) 2000-2025 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#pragma once


/* BEGIN IGNORE CODESTYLE */

/* Dynamic mock allows an individual test executable to control what a mock does.
 * T_MOCK_DYNAMIC_DECLARE()
 *   Declare a dynamic mock. This declaration should come in a header file under the mocks/ folder.
 *   The header file should be included in both the respective .c file and in the test .c file that
 *   wants to set the behaviour of the mock.
 *   It declares the signature of the mocked function so that if the signature changes the compiler
 *   can assure that the mock and its setters are in sync.
 * T_MOCK_DYNAMIC()
 *   Define the dynamic mock. This should come in a .c file under the mocks/ folder.
 *   This defines the mock function itself using the T_MOCK() macro.
 *
 * The test has 4 possible way to control the mock. It can temporarily set the return value,
 * it can set a temporary block callback, it can set a permanent return value or a permanent function.
 * @argument args_def is how the function arguments are defined in a function definition.
 *           This can be copy-pasted directly from the original function definition.
 * @argument args_invoke is how the same arguments are passed to a function call
 * @argument (optional) default_action should be a scope of code that will be executed if no mock control
 *           is set up. it can reference the arguments in args_def and also call the original
 *           function. If this argument is not supplied, the default action is to call the original XNU
 *           function with the same arguments.
 *
 * Example:
 * // we want to mock a function from XNU that has the signature:
 * size_t foobar(int a, char b);
 *
 * // in a header in the mocks library (tests/unit/mocks) add:
 * T_MOCK_DYNAMIC_DECLARE(size_t, foobar, (int a, char b));
 *
 * // in a .c file in the mock library (tests/unit/mocks) add:
 * T_MOCK_DYNAMIC(size_t, foobar, (int a, char b), (a, b), { return 0 });
 *
 * // Now to control the mock, in a T_DECL test you can do:
 * T_DECL(test, "test") {
 *     T_MOCK_SET_RETVAL(foobar, size_t, 42);
 *     // ... call into XNU which will call foobar()
 *
 *     T_MOCK_SET_CALLBACK(foobar, size_t, (int a, char b), {
 *         T_ASSERT_EQ(a, b, "args equal");
 *         return a + b;
 *     });
 *     // ... call into XNU which will call foobar()
 * }
 *
 * // The third option is to define a permanent return value for the mock that will
 * // be in effect for all tests in the executable.
 * // This essentially overrides the default-value that's defined in the T_MOCK_DYNAMIC()
 * T_MOCK_SET_PERM_RETVAL(foobar, size_t, 43);
 *
 * // The fourth option is for the test to define a permanent function in the global scope
 * // that will be called every time the mock is called.
 * T_MOCK_SET_PERM_FUNC(size_t, foobar, (int a, char b)) {
 *     return b - a;
 * }
 *
 * It's possible for multiple mock controls of different types to be active at the same time. The priority
 * in which the dynamic mock tries to find them is
 *   1. ret-val
 *   2. block call back
 *   3. permanent ret-val / permanent function
 * The effect of the ret-val and callback setters is limited to the scope the they are in. This
 * is achieved using a cleanup function in the setter.
 * It is possible for multiple setters of the same type to be invoked during the flow of the same scope.
 * In that case, the last setter that was invoked is in effect.
 *
 * It is not possible to have multiple static function setters and/or permanent ret-val setter for the
 * same mock in the same test executable. This would cause a compile/link error due to duplicate symbol.
 */

#define _T_MOCK_RETVAL_CALLBACK(name)       _mock_retval_callback_ ## name
#define _T_MOCK_CALLBACK(name)              _mock_callback_ ## name
#define _T_MOCK_PERM_RETVAL_FUNC(name)      _mock_p_retval_func_ ## name
#define _T_MOCK_PERM_FUNC(name)             _mock_func_ ## name

#define T_MOCK_DYNAMIC_DECLARE(ret, name, args_def)       \
    extern ret (^_T_MOCK_RETVAL_CALLBACK(name))(void);    \
    extern ret (^_T_MOCK_CALLBACK(name)) args_def;        \
    extern ret (*_T_MOCK_PERM_RETVAL_FUNC(name))(void);   \
    extern ret (*_T_MOCK_PERM_FUNC(name)) args_def;       \
    extern ret name args_def

#define _T_MOCK_DYNAMIC_WITH_IMPL(ret, name, args_def, args_invoke, default_action)  \
    ret (^_T_MOCK_RETVAL_CALLBACK(name)) (void) = NULL;                   \
    ret (^_T_MOCK_CALLBACK(name)) args_def = NULL;                        \
    ret (*_T_MOCK_PERM_RETVAL_FUNC(name)) (void) = NULL;                  \
    ret (*_T_MOCK_PERM_FUNC(name)) args_def = NULL;                       \
    T_MOCK(ret, name, args_def) {                                         \
        if (_T_MOCK_RETVAL_CALLBACK(name) != NULL) {                      \
            return _T_MOCK_RETVAL_CALLBACK(name)();                       \
        }                                                                 \
        if (_T_MOCK_CALLBACK(name) != NULL) {                             \
            return _T_MOCK_CALLBACK(name) args_invoke;                    \
        }                                                                 \
        if (_T_MOCK_PERM_RETVAL_FUNC(name) != NULL) {                     \
            return _T_MOCK_PERM_RETVAL_FUNC(name)();                      \
        }                                                                 \
        if (_T_MOCK_PERM_FUNC(name) != NULL) {                            \
            return _T_MOCK_PERM_FUNC(name) args_invoke;                   \
        }                                                                 \
        default_action;                                                   \
    }

#define _T_MOCK_DYNAMIC_DEFAULT_IMPL(ret, name, args_def, args_invoke) \
    _T_MOCK_DYNAMIC_WITH_IMPL(ret, name, args_def, args_invoke, { return name args_invoke; })

/* T_MOCK_DYNAMIC() selects which of the above versions to call depending on the number of arguments it gets
 * - T_MOCK_DYNAMIC(a, b, c, d) with 4 arguments expands to
 *   _T_MOCK_GET_INSTANCE(a, b, c, d, _T_MOCK_DYNAMIC_WITH_IMPL, _T_MOCK_DYNAMIC_DEFAULT_IMPL)(a, b, c, d)
 *   then NAME is _T_MOCK_DYNAMIC_DEFAULT_IMPL so this expands to
 *   _T_MOCK_DYNAMIC_DEFAULT_IMPL(a, b, c, d)
 * - T_MOCK_DYNAMIC(a, b, c, d, e) with 5 arguments expands to
 *   _T_MOCK_GET_INSTANCE(a, b, c, d, e, _T_MOCK_DYNAMIC_WITH_IMPL, _T_MOCK_DYNAMIC_DEFAULT_IMPL)(a, b, c, d, e)
 *   then NAME is _T_MOCK_DYNAMIC_WITH_IMPL so this expands to
 *   _T_MOCK_DYNAMIC_WITH_IMPL(a, b, c, e, e)
 */
#define _T_MOCK_GET_INSTANCE(_1, _2, _3, _4, _5, NAME, ...) NAME
#define T_MOCK_DYNAMIC(...) _T_MOCK_GET_INSTANCE(__VA_ARGS__, _T_MOCK_DYNAMIC_WITH_IMPL, _T_MOCK_DYNAMIC_DEFAULT_IMPL)(__VA_ARGS__)



#define _UT_CONCAT2(a, b) a ## b
#define _UT_CONCAT(a, b) _UT_CONCAT2(a, b)

static inline void
_mock_set_cleaner(void ***ptr) {
	**ptr = NULL;
}

/* How it works?
 * - For each mock that is defined using T_MOCK_DYNAMIC() the macro above defines a few
 * global variables with the function name suffixed, and also defines the mock function to check
 * these global variables.
 * - The test executable can then set any of them using the T_MOCK_SET_X() macros below
 * - T_MOCK_SET_RETVAL() and T_MOCK_SET_CALLBACK() should be used from inside T_DECL and have a
 * cleaner that undoes their effect at the end of the scope they are defined in.
 * The cleaner has a __COUNTER__ concatenated so that it's possible to have more than one such
 * T_MOCK_SET_X() invocation in the same scope
 * - T_MOCK_SET_PERM_RETVAL() and T_MOCK_SET_PERM_FUNC() should be used in the global scope
 * and has a constructor function that sets the global variable when the executable loads
 */

#define _T_MOCK_CLEANER(name) _UT_CONCAT(_cleaner_ ## name, __COUNTER__)
#define _T_MOCK_RETVAL_CAPTURE(name, N) _UT_CONCAT(_mock_retval_capture_ ## name, N)

/* to set a return value, we set a global that holds a callback block that returns the value.
 * The callback variable is a pointer and NULL indicates it's not set
 * The value expression the user gives is first captured in a local variable since some
 * expressions can't be captured by a block (array reference for instance) */
#define _T_MOCK_SET_RETVAL_IMPL(name, ret, val, N)                                              \
        ret _T_MOCK_RETVAL_CAPTURE(name, N) = val;                                              \
        _T_MOCK_RETVAL_CALLBACK(name) = ^ret(void) { return _T_MOCK_RETVAL_CAPTURE(name, N); }; \
        __attribute__((cleanup(_mock_set_cleaner))) void **_T_MOCK_CLEANER(name) =              \
            (void**)&_T_MOCK_RETVAL_CALLBACK(name)
#define T_MOCK_SET_RETVAL(name, ret, val) _T_MOCK_SET_RETVAL_IMPL(name, ret, val, __COUNTER__)

/* to set a mock callback block from the user we set a dedicated callback for that, so it doesn't
 * interfere with SET_RETVAL */
#define T_MOCK_SET_CALLBACK(name, ret, args_def, body)                              \
        _T_MOCK_CALLBACK(name) = ^ret args_def body;                                \
        __attribute__((cleanup(_mock_set_cleaner))) void **_T_MOCK_CLEANER(name) =  \
            (void**)&_T_MOCK_CALLBACK(name)

#define _T_MOCK_CTOR_SETTER(name) _ctor_setter_ ## name
#define _T_MOCK_PERM_HOOK(name)   PERM_HOOK_ ## name

/* To set a permanent return value, we define a function that returns it, and set it to the
 * extern global in a constructor.
 * This setter needs to be in the global scope of the tester */
#define T_MOCK_SET_PERM_RETVAL(name, ret, val)                          \
        ret _T_MOCK_PERM_HOOK(name)(void) { return (val); }             \
        __attribute__((constructor)) void _T_MOCK_CTOR_SETTER(name)() { \
            _T_MOCK_PERM_RETVAL_FUNC(name) = _T_MOCK_PERM_HOOK(name);   \
        }

/* To set a permanent function that will be called from the mock we declare it, set it to the extern
 * in a constructor and define it.
 * This needs to be in the global scope and the body of the function needs to follows it immediately */
#define T_MOCK_SET_PERM_FUNC(ret, name, args_def)                        \
        ret _T_MOCK_PERM_HOOK(name) args_def;                            \
        __attribute__((constructor)) void _T_MOCK_CTOR_SETTER(name)() {  \
            _T_MOCK_PERM_FUNC(name) = _T_MOCK_PERM_HOOK(name);           \
        }                                                                \
        ret _T_MOCK_PERM_HOOK(name) args_def


/* T_MOCK_CALL_QUEUE()
 *   Allow tests to define a call expectation queue for a mock
 *
 * This macro wraps a definition of a struct and defines easy helpers to
 * manage a global queue of elements of that struct.
 * A test can use this along with a mock callback to verify and control what the mock
 * does in every call it gets.
 * @argument type_name the name of the struct to define
 * @argument struct_body the elements of the struct
 *
 * Example:
 * // for mocking the function foobar() we'll define a struct that will allow the mock
 * // to verify its arguments and control its return value. The elements of the struct can
 * // be anything.
 * T_MOCK_CALL_QUEUE(fb_call, {
 *     int expected_a_eq;
 *     bool expected_b_small;
 *     size_t ret_val;
 * })
 *
 * T_MOCK_SET_PERM_FUNC(size_t, foobar, (int a, char b)) {
 *     fb_call call = dequeue_fb_call();
 *     T_ASSERT_EQ(a, call.expected_a_eq, "a arg");
 *     if (call.expected_b_small)
 *         T_ASSERT_LE(b, 127, "b arg too big");
 *     return call.ret_val;
 * }
 *
 * // in the test we set up the expected calls before calling the code that ends up in the mock
 * T_DECL(test, "test") {
 *     enqueue_fb_call( (fb_call){ .expected_a = 1, .expected_b = 2, .ret_val = 3 });
 * 	   enqueue_fb_call( (fb_call){ .expected_a = 10, .expected_b = 20, .ret_val = 30 });
 *     // ... call into XNU which will call foobar()
 *     assert_empty_fb_call(); // check all calls were consumed
 * }
 */

#define _T_MOCK_CALL_LST(type_name)  _lst_ ## type_name

#define T_MOCK_CALL_QUEUE(type_name, struct_body)                                         \
    typedef struct s_ ## type_name struct_body type_name;                                 \
    struct _node_ ## type_name {                                                          \
        STAILQ_ENTRY(_node_ ## type_name) next;                                           \
        type_name d;                                                                      \
    };                                                                                    \
    static STAILQ_HEAD(, _node_ ## type_name) _T_MOCK_CALL_LST(type_name) =               \
        STAILQ_HEAD_INITIALIZER(_T_MOCK_CALL_LST(type_name));                             \
    static void enqueue_ ## type_name (type_name value) {                                 \
        struct _node_ ## type_name *node = calloc(1, sizeof(struct _node_ ## type_name)); \
        node->d = value;                                                                  \
        STAILQ_INSERT_TAIL(&_T_MOCK_CALL_LST(type_name), node, next);                     \
    }                                                                                     \
    static type_name dequeue_ ## type_name (void) {                                       \
        struct _node_ ## type_name *node = STAILQ_FIRST(&_T_MOCK_CALL_LST(type_name));    \
        T_QUIET; T_ASSERT_NOTNULL(node, "consumed too many " #type_name);                 \
        type_name d = node->d;                                                            \
        STAILQ_REMOVE_HEAD(&_T_MOCK_CALL_LST(type_name), next);                           \
        free(node);                                                                       \
        return d;                                                                         \
    }                                                                                     \
    static void assert_empty_ ## type_name (void) {                                       \
        T_QUIET; T_ASSERT_TRUE( STAILQ_EMPTY(&_T_MOCK_CALL_LST(type_name)),               \
                  "calls not fully consumed " #type_name);                                \
    }                                                                                     \
    static void clear_ ## type_name (void) {                                              \
        STAILQ_INIT(&_T_MOCK_CALL_LST(type_name));                                        \
    }

/* END IGNORE CODESTYLE */
