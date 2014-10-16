#include <security_utilities/devrandom.h>

#include "testcpp.h"

int main(int argc, char *const *argv)
{
	plan_tests(4);

	DevRandomGenerator rnd;
	char buf[8]  = {};
	no_throw(rnd.random(buf, sizeof(buf)), "read data from rnd.");
	TODO: {
		todo("writing to read only rnd succeeeds unexpectedly.");

		does_throw(rnd.addEntropy(buf, sizeof(buf)),
			"writing to rnd throws");
	}


	DevRandomGenerator rndw(true);
	no_throw(rndw.random(buf, sizeof(buf)), "read data from rndw.");
	no_throw(rndw.addEntropy(buf, sizeof(buf)), "write data to rndw.");
}
