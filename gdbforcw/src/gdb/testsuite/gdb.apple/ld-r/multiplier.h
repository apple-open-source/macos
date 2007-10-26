#ifdef USE_LONG
  #define LONG_TYPE long
  #define GET_MULTIPLIER get_multiplier_long
#else
  #define LONG_TYPE long long
  #define GET_MULTIPLIER get_multiplier_long_long
#endif

struct input_type
{
  int first;
  LONG_TYPE second;
};

LONG_TYPE GET_MULTIPLIER (struct input_type *);
