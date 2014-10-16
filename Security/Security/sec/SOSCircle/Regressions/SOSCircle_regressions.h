/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app in the Release and Debug schemes
 */
#include <test/testmore.h>

ONE_TEST(sc_20_keynames)
ONE_TEST(sc_30_peerinfo)
ONE_TEST(sc_31_peerinfo)
ONE_TEST(sc_40_circle)
OFF_ONE_TEST(sc_41_cloudcircle) // This is destructive to the cloud state
ONE_TEST(sc_42_circlegencount)
ONE_TEST(sc_45_digestvector)
OFF_ONE_TEST(sc_50_message)
OFF_ONE_TEST(sc_51_persistentEC)
OFF_ONE_TEST(sc_60_peer)
#ifdef NO_SERVER
OFF_ONE_TEST(sc_70_engine)
OFF_ONE_TEST(sc_75_circle_engine)
#else
OFF_ONE_TEST(sc_70_engine)
ONE_TEST(sc_75_circle_engine)
#endif

OFF_ONE_TEST(sc_90_ckdclient)
OFF_ONE_TEST(sc_95_ckd2client)
OFF_ONE_TEST(sc_103_syncupdate)
ONE_TEST(sc_130_resignationticket)

