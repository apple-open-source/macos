extern int bad_error;
extern int arch_multiple;

extern void check_for_ProjectBuilder(
    void);

extern void as_warn(
    const char *format,
     ...) __attribute__ ((format (printf, 1, 2)));

extern void as_warning(
    const char *format,
     ...) __attribute__ ((format (printf, 1, 2)));

extern void as_bad(
    const char *format,
     ...) __attribute__ ((format (printf, 1, 2)));

extern void as_fatal(
    const char *format,
     ...) __attribute__ ((format (printf, 1, 2)));
