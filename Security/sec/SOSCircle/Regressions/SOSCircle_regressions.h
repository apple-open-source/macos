/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app in the Release and Debug schemes
 */
#include <test/testmore.h>

ONE_TEST(sc_20_keynames)
ONE_TEST(sc_30_peerinfo)
ONE_TEST(sc_40_circle)
OFF_ONE_TEST(sc_41_cloudcircle) // This is destructive to the cloud state
OFF_ONE_TEST(sc_51_persistentEC)

#ifdef NO_SERVER
ONE_TEST(sc_60_peer)
ONE_TEST(sc_70_engine)
ONE_TEST(sc_75_circle_engine)
#else
OFF_ONE_TEST(sc_60_peer)
OFF_ONE_TEST(sc_70_engine)
OFF_ONE_TEST(sc_75_circle_engine)
#endif

OFF_ONE_TEST(sc_90_ckdclient)
OFF_ONE_TEST(sc_95_ckd2client)
OFF_ONE_TEST(sc_100_devicecircle)
OFF_ONE_TEST(sc_101_accountsync)
OFF_ONE_TEST(sc_102_cfusernotification)
OFF_ONE_TEST(sc_103_syncupdate)
OFF_ONE_TEST(sc_120_cloudcircle)
ONE_TEST(sc_130_resignationticket)


