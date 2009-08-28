/*
 * This tests the core_profile() function. When it was introduced, it wouldn't
 * compile due to the number of arguments exposing various D compiler bugs.
 * The test is a success if it compiles at all.
 */

BEGIN
{
	core_profile(COREPROFILE_SIGNPOST_POINT, 1024, 1, 2, 3, 4, 5);
	exit(0);
}
