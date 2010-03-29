extern cchar_t * _nc_wacs;
#define NCURSES_WACS(c)	(&_nc_wacs[(unsigned char)c])
#define WACS_BSSB	NCURSES_WACS('l')
#define WACS_SSBB	NCURSES_WACS('m')
#define WACS_BBSS	NCURSES_WACS('k')
#define WACS_SBBS	NCURSES_WACS('j')
#define WACS_SBSS	NCURSES_WACS('u')
#define WACS_SSSB	NCURSES_WACS('t')
#define WACS_SSBS	NCURSES_WACS('v')
#define WACS_BSSS	NCURSES_WACS('w')
#define WACS_BSBS	NCURSES_WACS('q')
#define WACS_SBSB	NCURSES_WACS('x')
#define WACS_SSSS	NCURSES_WACS('n')
#define WACS_ULCORNER	WACS_BSSB
#define WACS_LLCORNER	WACS_SSBB
#define WACS_URCORNER	WACS_BBSS
#define WACS_LRCORNER	WACS_SBBS
#define WACS_RTEE	WACS_SBSS
#define WACS_LTEE	WACS_SSSB
#define WACS_BTEE	WACS_SSBS
#define WACS_TTEE	WACS_BSSS
#define WACS_HLINE	WACS_BSBS
#define WACS_VLINE	WACS_SBSB
#define WACS_PLUS	WACS_SSSS
#define WACS_S1		NCURSES_WACS('o') /* scan line 1 */
#define WACS_S9 	NCURSES_WACS('s') /* scan line 9 */
#define WACS_DIAMOND	NCURSES_WACS('`') /* diamond */
#define WACS_CKBOARD	NCURSES_WACS('a') /* checker board */
#define WACS_DEGREE	NCURSES_WACS('f') /* degree symbol */
#define WACS_PLMINUS	NCURSES_WACS('g') /* plus/minus */
#define WACS_BULLET	NCURSES_WACS('~') /* bullet */
#define WACS_LARROW	NCURSES_WACS(',') /* arrow left */
#define WACS_RARROW	NCURSES_WACS('+') /* arrow right */
#define WACS_DARROW	NCURSES_WACS('.') /* arrow down */
#define WACS_UARROW	NCURSES_WACS('-') /* arrow up */
#define WACS_BOARD	NCURSES_WACS('h') /* board of squares */
#define WACS_LANTERN	NCURSES_WACS('i') /* lantern symbol */
#define WACS_BLOCK	NCURSES_WACS('0') /* solid square block */
#define WACS_S3		NCURSES_WACS('p') /* scan line 3 */
#define WACS_S7		NCURSES_WACS('r') /* scan line 7 */
#define WACS_LEQUAL	NCURSES_WACS('y') /* less/equal */
#define WACS_GEQUAL	NCURSES_WACS('z') /* greater/equal */
#define WACS_PI		NCURSES_WACS('{') /* Pi */
#define WACS_NEQUAL	NCURSES_WACS('|') /* not equal */
#define WACS_STERLING	NCURSES_WACS('}') /* UK pound sign */
