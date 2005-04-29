/* APPLE LOCAL file AltiVec 3840704 */
/* { dg-do compile { target powerpc*-*-* } } */
/* { dg-options "-faltivec" } */
/* { dg-final { scan-assembler-not "stvx" } } */

void *memset(void *, int, unsigned long int);

struct tm 
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  long tm_gmtoff;
  char *tm_zone;
};


int asl_send()
{
  char *str, *out;
  unsigned int i, len, level, outstatus;
  struct tm gtime;

  memset(&gtime, 0, sizeof(struct tm));  /* { dg-warning "disabled" } */
  return 0;
}

