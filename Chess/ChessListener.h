#ifdef __cplusplus
extern "C" {
#endif

#define CHESS_DEBUG 1

void CL_Listen(short color, short pieces[], short colors[]);
void CL_DontListen();
void CL_MakeMove(const char * move);
void CL_SetHelp(unsigned len, const void * data);

#ifdef __cplusplus
}
#endif
