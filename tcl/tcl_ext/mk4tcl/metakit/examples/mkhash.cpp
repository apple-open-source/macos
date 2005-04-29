/* Hash timing test harness
 *
 * Usage: mkhash opts count
 *
 * 	opts is a combination of the following flags (default "Dhs"):
 * 		d	store data view flat
 * 		D	store data view blocked
 * 		m	store map view flat
 * 		M	store map view blocked
 * 		h	use hashing
 * 		H	use hashing, after filling the view
 * 		o	use ordering
 * 		2	2-key hashing/ordering, instead of 1-key
 * 		s	time small (2-row add) commits, i.s.o. 1000 rows
 * 		f	do frequent commits, every 10 i.s.o. 1000 rows
 *
 * 	count is the total number of rows added, default is 250,000
 *      (it should be a multiple of 10,000 for proper reporting)
 *
 *  % g++ -Dq4_INLINE mkhash.cpp -lmk4
 *  % for i in - d D dh Dh dmh Dmh do Do; do rm -f test.dat; a.out $i; done
 *  [...]
 */

#include <mk4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>

  long ticks ()
  {
    LARGE_INTEGER t;

    static double f = 0.0;
    if (f == 0.0) {
      QueryPerformanceFrequency(&t);
      f = (double) t.QuadPart / 1000000.0;
    }

    QueryPerformanceCounter(&t);
    return (long) (f * t.QuadPart);
  }
#else
  #include <sys/time.h>
  #include <sys/types.h>
  #include <sys/stat.h>

  long ticks()
  {
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return tv.tv_sec * 1000000 + tv.tv_usec;
  }
#endif

int main(int argc, char **argv)
{
  char buf [25];
  c4_Row row;
  long t;
  int nkeys = 1;

  //setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

  c4_Storage storage ("test.dat", true);

  c4_StringProp pKey ("key"), pKey2 ("key2");
  c4_View data = pKey;

  c4_IntProp pH ("_H"), pR ("_R");
  c4_View map = (pH, pR);
  const char *s = argc > 1 ? argv[1] : "Dhs";

  if (strchr(s, '2')) {
    // must create properties in same order as in the hash view (ouch!)
    pKey (row) = "";
    pKey2 (row) = "abcdefghijklmnopqrstuvwxyz";
    nkeys = 2;
    if (strchr(s, 'd'))
      data = storage.GetAs("data[key:S,key2:S]");
    if (strchr(s, 'D')) {
      data = storage.GetAs("data[_B[key:S,key2:S]]");
      data = data.Blocked();
    }
  } else {
    if (strchr(s, 'd'))
      data = storage.GetAs("data[key:S]");
    if (strchr(s, 'D')) {
      data = storage.GetAs("data[_B[key:S]]");
      data = data.Blocked();
    }
  }

  if (strchr(s, 'm'))
    map = storage.GetAs("map[_H:I,_R:I]");
  if (strchr(s, 'M')) {
    map = storage.GetAs("map[_B[_H:I,_R:I]]");
    map = map.Blocked();
  }
  if (strchr(s, 'h'))
    data = data.Hash(map, nkeys);
  if (strchr(s, 'o'))
    data = data.Ordered(nkeys);
  int cfreq = strchr(s, 'f') ? 10 : 1000;

  int limit = 250000;
  if (argc > 2)
    limit = atoi(argv[2]);

  puts(s);
  puts("      ROW       ADD         FIND       COMMIT         SIZE");

  for (int i = 1; i <= limit; ++i) {
    sprintf(buf, "%10d%10d", 100000000 + i, 200000000 + i);

    pKey (row) = buf;

    t = ticks();
    data.Add(row);
    long at = ticks() - t;

    if ((i+2) % cfreq == 0 && strchr(s, 's'))
      storage.Commit();

    if (i % cfreq == 0) {
      t = ticks();
      storage.Commit();
      long ct = ticks() - t;

      if (i % (limit/10) == 0) {
	t = ticks();
	int n = data.Find(row);
	long ft = ticks() - t;
	      
	if (n < 0) { puts(buf); return 1; }

	printf("%9d %9d uS %9d uS %9d uS %9d bytes \n",
	    	i, at, ft, ct, storage.Strategy().FileSize());
      }
    }
  }

  if (strchr(s, 'H')) {
    t = ticks();
    data = data.Hash(map, nkeys);
    long ht = ticks() - t;

    t = ticks();
    storage.Commit();
    long ct = ticks() - t;

    printf("construct hash: %d uS, then commit: %d uS\n", ht, ct);
    fflush(stdout);
  }

  return 0;
}
