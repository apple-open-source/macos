//
// Tests for
//  template <typename U>
//  intrusive_shared_ptr(intrusive_shared_ptr<U, RefcountPolicy>&& other);
//
//  intrusive_shared_ptr(intrusive_shared_ptr&& other);
//

#include <libkern/c++/intrusive_shared_ptr.h>
#include <darwintest.h>
#include <darwintest_utils.h>
#include <type_traits>
#include <utility>
#include "test_policy.h"

struct Base { int i; };
struct Derived : Base { };

struct Base1 { int i; };
struct Base2 { long l; };
struct DerivedMultiple : Base1, Base2 {
	DerivedMultiple(int i) : Base1{i}, Base2{i + 10}
	{
	}
};

struct Unrelated { };

template <typename Stored, typename From, typename To>
static void
tests()
{
	Stored obj{3};

	// Test with non-null pointers
	{
		test_policy::retain_count = 0;
		libkern::intrusive_shared_ptr<From, test_policy> from(&obj, libkern::retain);
		CHECK(test_policy::retain_count == 1);
		CHECK(from.get() == &obj);

		libkern::intrusive_shared_ptr<To, test_policy> to(std::move(from)); // explicit
		CHECK(test_policy::retain_count == 1);
		CHECK(to.get() == &obj);
		CHECK(from.get() == nullptr);
	}
	{
		test_policy::retain_count = 0;
		libkern::intrusive_shared_ptr<From, test_policy> from(&obj, libkern::retain);
		CHECK(test_policy::retain_count == 1);
		CHECK(from.get() == &obj);

		libkern::intrusive_shared_ptr<To, test_policy> to{std::move(from)}; // explicit
		CHECK(test_policy::retain_count == 1);
		CHECK(to.get() == &obj);
		CHECK(from.get() == nullptr);
	}
	{
		test_policy::retain_count = 0;
		libkern::intrusive_shared_ptr<From, test_policy> from(&obj, libkern::retain);
		CHECK(test_policy::retain_count == 1);
		CHECK(from.get() == &obj);

		libkern::intrusive_shared_ptr<To, test_policy> to = std::move(from); // implicit
		CHECK(test_policy::retain_count == 1);
		CHECK(to.get() == &obj);
		CHECK(from.get() == nullptr);
	}

	// Test with a null pointer
	{
		test_policy::retain_count = 3;
		libkern::intrusive_shared_ptr<From, test_policy> from = nullptr;
		libkern::intrusive_shared_ptr<To, test_policy> to = std::move(from);
		CHECK(test_policy::retain_count == 3);
		CHECK(to.get() == nullptr);
		CHECK(from.get() == nullptr);
	}
}

T_DECL(ctor_move, "intrusive_shared_ptr.ctor.move", T_META_TAG_VM_PREFERRED) {
	tests</*stored*/ Derived, /*from*/ Derived, /*to*/ Derived>();
	tests</*stored*/ Derived, /*from*/ Derived, /*to*/ Derived const>();
	tests</*stored*/ Derived, /*from*/ Derived const, /*to*/ Derived const>();

	tests</*stored*/ Derived, /*from*/ Derived, /*to*/ Base>();
	tests</*stored*/ Derived, /*from*/ Derived, /*to*/ Base const>();
	tests</*stored*/ Derived, /*from*/ Derived const, /*to*/ Base const>();

	tests</*stored*/ DerivedMultiple, /*from*/ DerivedMultiple, /*to*/ Base1>();
	tests</*stored*/ DerivedMultiple, /*from*/ DerivedMultiple const, /*to*/ Base1 const>();

	tests</*stored*/ DerivedMultiple, /*from*/ DerivedMultiple, /*to*/ Base2>();
	tests</*stored*/ DerivedMultiple, /*from*/ DerivedMultiple const, /*to*/ Base2 const>();

	// Make sure basic trait querying works
	static_assert(std::is_move_constructible_v<test_shared_ptr<Derived> >);

	// Make sure downcasts are disabled
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<Derived>, /*from*/ test_shared_ptr<Base>&&>);
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<DerivedMultiple>, /*from*/ test_shared_ptr<Base1>&&>);
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<DerivedMultiple>, /*from*/ test_shared_ptr<Base2>&&>);
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<Base2>, /*from*/ test_shared_ptr<Base1>&&>);

	// Make sure const-casting away doesn't work
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<Derived>, /*from*/ test_shared_ptr<Derived const>&&>);

	// Make sure casting to unrelated types doesn't work
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<char>, /*from*/ test_shared_ptr<Derived>&&>);
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<Unrelated>, /*from*/ test_shared_ptr<Derived>&&>);
	static_assert(!std::is_constructible_v</*to*/ test_shared_ptr<Base2>, /*from*/ test_shared_ptr<Base1>&&>);

	// Make sure constructing with different policies doesn't work
	static_assert(!std::is_constructible_v</*to*/ libkern::intrusive_shared_ptr<Derived, dummy_policy<2> >, /*from*/ libkern::intrusive_shared_ptr<Derived, dummy_policy<1> >&&>);
}
