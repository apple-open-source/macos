TT_Raster_Map *makeBitmap(unsigned short*, char*, double, int, int);
void writeBanner(TT_Raster_Map *);
void FTError(char*, TT_Error);
void Error(char*);
int find_unicode_cmap(TT_Face, TT_CharMap*);
unsigned short *l1toUnicode(char *);
unsigned short *l2toUnicode(char *);
unsigned short *UTF8toUnicode(char *);
void usage(void);
