/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/ICS1562.c,v 1.3 1999/04/17 07:06:56 dawes Exp $ */

#define BASE_FREQ 14.31818
#define MAX_FREQ 230000

#define	reorder(a)	( \
	(a & 0x80) >> 7 | \
	(a & 0x40) >> 5 | \
	(a & 0x20) >> 3 | \
	(a & 0x10) >> 1 | \
	(a & 0x08) << 1 | \
	(a & 0x04) << 3 | \
	(a & 0x02) << 5 | \
	(a & 0x01) << 7 )

#define CHECK_MIN(m,a,r,n)  do { \
      diff = f - (BASE_FREQ * 1e3 * (n)) / (r << p); \
      if (diff < 0) diff = -diff; \
      if (diff < min_diff) {  \
	min_diff = diff; \
	best_m = m; \
	best_a = a; \
	best_r = r; \
      } \
    } while(0)


static void Set_1562_PLL(int f, int p, int m, int a, int r, unsigned char *bits)
{
  bits[0] = 0x80;  /* N1 = 4, not used for PLL */
  bits[1] = reorder(p) >> 4;
  bits[2] = 0x00;  /* N2 = 1 */
  if (f <= 120000) 
    bits[3] = 0x20;  /* V = 100 */
  else if (f <= 200000) 
    bits[3] = 0xa0;  /* V = 101 */
  else
    bits[3] = 0x60;  /* V = 110 */
  bits[3] |= 0x05;   /* P = 10, phase detector on */
  bits[4] = reorder(m);
  bits[5] = reorder(a);
  bits[6] = reorder(r);

#ifdef DEBUG
  { 
    int i;
    for(i=0; i<7; i++)
      ErrorF("%02x ", bits[i]);
    ErrorF("\n");
  }
#endif
}

void ICS1562_CalcClockBits(long f, unsigned char *bits);

void ICS1562_CalcClockBits(long f, unsigned char *bits)
{
  int n,r, a,m, p, r0,r1,n0,n1;
  int best_m=34, best_a=1, best_r=30;
  double diff, min_diff;
  double ff, ffp;

  if (f > MAX_FREQ)
    f = MAX_FREQ;

  if (f >= MAX_FREQ/2) 
    p=0;
  else if (f >= MAX_FREQ/4) 
    p=1;
  else 
    p=2;

  ff  = f / 1e3 / BASE_FREQ;
  ffp = ff * (1 << p);
  min_diff = 999999999;

  r0 = (int)(7/ffp);
  if (r0 < 1) r0 = 1;
  r1 = (int)(449/ffp);
  if (r1 > 0x7f+1) r1 = 0x7f+1;
  if (r1 < r0) r1 = r0;

  for (r=r0; r<r1; r++) {
    n0 = (int)(ffp * r);
    if (n0 < 7) n0 = 7;
    n1 = (int)(ffp * (r+1));
    if (n1 > 448) n1 = 448;
    for (n=n0; n<n1; n++) {
      m = ((n+3)/7) - 1;
      if (m   <= 0x3f) CHECK_MIN(m, 0, r, (m+1)*7);
      if (++m <= 0x3f) CHECK_MIN(m, 0, r, (m+1)*7);
      m = (n/6) - 1;
      a = n - (m+1)*6;
      if (m <= 0x3f && a > 0 && a<=7) CHECK_MIN(m, a, r, (m+1)*6+a);
      m++;
      a = n - (m+1)*6;
      if (m <= 0x3f && a > 0 && a<=7) CHECK_MIN(m, a, r, (m+1)*6+a);
    }
  }

  if (best_a) n = (best_m+1)*6+best_a;
  else n = (best_m+1)*7;

#ifdef DEBUG
  ErrorF("%8.3f %f %f  p=%d m=%d a=%d r=%d  %d/%d\n"
	 ,f/1e3
	 ,min_diff,(BASE_FREQ * (n)) / (best_r << p)
	 ,p ,best_m, best_a, best_r-1
	 ,n,best_r
	 );
#endif
  
  Set_1562_PLL(f, p, best_m, best_a, best_r-1, bits);
}
