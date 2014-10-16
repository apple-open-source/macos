#include <security_utilities/errors.h>

#include "testcpp.h"

using namespace Security;

int main(int argc, char *const *argv)
{
	plan_tests(5);

	no_throw(, "doesn't throw");
	does_throw(throw 1, "does throw");
	does_throw(UnixError::throwMe(1), "throws something.");
	is_throw(UnixError::throwMe(1), CommonError, unixError(), 1,
		"throws UnixError 1");
	is_throw(UnixError::throwMe(1), CommonError, osStatus(), 100001,
		"throws osStatus 100001");
}
