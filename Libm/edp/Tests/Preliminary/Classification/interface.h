#if defined __cplusplus
extern "C" {
#endif


int fpclassifyF(float x);
int fpclassifyD(double x);
int fpclassifyL(long double x);

int isnormalF(float x);
int isnormalD(double x);
int isnormalL(long double x);

int isfiniteF(float x);
int isfiniteD(double x);
int isfiniteL(long double x);

int isinfF(float x);
int isinfD(double x);
int isinfL(long double x);

int isnanF(float x);
int isnanD(double x);
int isnanL(long double x);

int signbitF(float x);
int signbitD(double x);
int signbitL(long double x);


#if defined __cplusplus
}	// Close extern "C" {.
#endif
