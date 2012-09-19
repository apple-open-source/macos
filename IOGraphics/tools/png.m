//cc png.m -std=c99 -framework AppKit
//  Utility for converting .PNG images into compact LZSS-packed format
//  with CLUT tables.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <stdlib.h>
#import <stdint.h>
#import <string.h>

/**************************************************************
LZSS.C -- A Data Compression Program
***************************************************************
    4/6/1989 Haruhiko Okumura
    Use, distribute, and modify this program freely.
    Please send me your improved versions.
        PC-VAN      SCIENCE
        NIFTY-Serve PAF01022
        CompuServe  74050,1022
**************************************************************/
#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
                           if match_length is greater than this */
#define NIL       N     /* index for root of binary search trees */

struct encode_state {
    /*
     * left & right children & parent. These constitute binary search trees.
     */
    int lchild[N + 1], rchild[N + 257], parent[N + 1];
    /* ring buffer of size N, with extra F-1 bytes to aid string comparison */
    u_int8_t text_buf[N + F - 1];
    /*
     * match_length of longest match.
     * These are set by the insert_node() procedure.
     */
    int match_position, match_length;
};

/*
 * initialize state, mostly the trees
 *
 * For i = 0 to N - 1, rchild[i] and lchild[i] will be the right and left
 * children of node i.  These nodes need not be initialized.  Also, parent[i]
 * is the parent of node i.  These are initialized to NIL (= N), which stands
 * for 'not used.'  For i = 0 to 255, rchild[N + i + 1] is the root of the
 * tree for strings that begin with character i.  These are initialized to NIL.
 * Note there are 256 trees.
 */
static void init_state(struct encode_state *sp)
{
    int  i;
    bzero(sp, sizeof(*sp));
    for (i = 0; i < N - F; i++)
        sp->text_buf[i] = ' ';
    for (i = N + 1; i <= N + 256; i++)
        sp->rchild[i] = NIL;
    for (i = 0; i < N; i++)
        sp->parent[i] = NIL;
}

/*
 * Inserts string of length F, text_buf[r..r+F-1], into one of the trees
 * (text_buf[r]'th tree) and returns the longest-match position and length
 * via the global variables match_position and match_length.
 * If match_length = F, then removes the old node in favor of the new one,
 * because the old one will be deleted sooner. Note r plays double role,
 * as tree node and position in buffer.
 */
static void insert_node(struct encode_state *sp, int r)
{
    int  i, p, cmp;
    u_int8_t  *key;
    cmp = 1;
    key = &sp->text_buf[r];
    p = N + 1 + key[0];
    sp->rchild[r] = sp->lchild[r] = NIL;
    sp->match_length = 0;
    for ( ; ; ) {
        if (cmp >= 0) {
            if (sp->rchild[p] != NIL)
                p = sp->rchild[p];
            else {
                sp->rchild[p] = r;
                sp->parent[r] = p;
                return;
            }
        } else {
            if (sp->lchild[p] != NIL)
                p = sp->lchild[p];
            else {
                sp->lchild[p] = r;
                sp->parent[r] = p;
                return;
            }
        }
        for (i = 1; i < F; i++) {
            if ((cmp = key[i] - sp->text_buf[p + i]) != 0)
                break;
        }
        if (i > sp->match_length) {
            sp->match_position = p;
            if ((sp->match_length = i) >= F)
                break;
        }
    }
    sp->parent[r] = sp->parent[p];
    sp->lchild[r] = sp->lchild[p];
    sp->rchild[r] = sp->rchild[p];
    sp->parent[sp->lchild[p]] = r;
    sp->parent[sp->rchild[p]] = r;
    if (sp->rchild[sp->parent[p]] == p)
        sp->rchild[sp->parent[p]] = r;
    else
        sp->lchild[sp->parent[p]] = r;
    sp->parent[p] = NIL;  /* remove p */
}

/* deletes node p from tree */
static void delete_node(struct encode_state *sp, int p)
{
    int  q;
    
    if (sp->parent[p] == NIL)
        return;  /* not in tree */
    if (sp->rchild[p] == NIL)
        q = sp->lchild[p];
    else if (sp->lchild[p] == NIL)
        q = sp->rchild[p];
    else {
        q = sp->lchild[p];
        if (sp->rchild[q] != NIL) {
            do {
                q = sp->rchild[q];
            } while (sp->rchild[q] != NIL);
            sp->rchild[sp->parent[q]] = sp->lchild[q];
            sp->parent[sp->lchild[q]] = sp->parent[q];
            sp->lchild[q] = sp->lchild[p];
            sp->parent[sp->lchild[p]] = q;
        }
        sp->rchild[q] = sp->rchild[p];
        sp->parent[sp->rchild[p]] = q;
    }
    sp->parent[q] = sp->parent[p];
    if (sp->rchild[sp->parent[p]] == p)
        sp->rchild[sp->parent[p]] = q;
    else
        sp->lchild[sp->parent[p]] = q;
    sp->parent[p] = NIL;
}

u_int8_t *compress_lzss(
    u_int8_t       * dst,
    u_int32_t        dstlen,
    u_int8_t       * src,
    u_int32_t        srclen)
{
    u_int8_t * result = NULL;
    /* Encoding state, mostly tree but some current match stuff */
    struct encode_state *sp;
    int  i, c, len, r, s, last_match_length, code_buf_ptr;
    u_int8_t code_buf[17], mask;
    u_int8_t * srcend = src + srclen;
    u_int8_t *dstend = dst + dstlen;
    /* initialize trees */
    sp = (struct encode_state *) malloc(sizeof(*sp));
    if (!sp) goto finish;
    init_state(sp);
    /*
     * code_buf[1..16] saves eight units of code, and code_buf[0] works
     * as eight flags, "1" representing that the unit is an unencoded
     * letter (1 byte), "0" a position-and-length pair (2 bytes).
     * Thus, eight units require at most 16 bytes of code.
     */
    code_buf[0] = 0;
    code_buf_ptr = mask = 1;
    /* Clear the buffer with any character that will appear often. */
    s = 0;  r = N - F;
    /* Read F bytes into the last F bytes of the buffer */
    for (len = 0; len < F && src < srcend; len++)
        sp->text_buf[r + len] = *src++;  
    if (!len)
        goto finish;
    /*
     * Insert the F strings, each of which begins with one or more
     * 'space' characters.  Note the order in which these strings are
     * inserted.  This way, degenerate trees will be less likely to occur.
     */
    for (i = 1; i <= F; i++)
        insert_node(sp, r - i);
    /*
     * Finally, insert the whole string just read.
     * The global variables match_length and match_position are set.
     */
    insert_node(sp, r);
    do {
        /* match_length may be spuriously long near the end of text. */
        if (sp->match_length > len)
            sp->match_length = len;
        if (sp->match_length <= THRESHOLD) {
            sp->match_length = 1;  /* Not long enough match.  Send one byte. */
            code_buf[0] |= mask;  /* 'send one byte' flag */
            code_buf[code_buf_ptr++] = sp->text_buf[r];  /* Send uncoded. */
        } else {
            /* Send position and length pair. Note match_length > THRESHOLD. */
            code_buf[code_buf_ptr++] = (u_int8_t) sp->match_position;
            code_buf[code_buf_ptr++] = (u_int8_t)
                ( ((sp->match_position >> 4) & 0xF0)
                |  (sp->match_length - (THRESHOLD + 1)) );
        }
        if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
                /* Send at most 8 units of code together */
            for (i = 0; i < code_buf_ptr; i++)
                if (dst < dstend)
                    *dst++ = code_buf[i];
                else
                    goto finish;
            code_buf[0] = 0;
            code_buf_ptr = mask = 1;
        }
        last_match_length = sp->match_length;
        for (i = 0; i < last_match_length && src < srcend; i++) {
            delete_node(sp, s);    /* Delete old strings and */
            c = *src++;
            sp->text_buf[s] = c;    /* read new bytes */
            /*
             * If the position is near the end of buffer, extend the buffer
             * to make string comparison easier.
             */
            if (s < F - 1)
                sp->text_buf[s + N] = c;
            /* Since this is a ring buffer, increment the position modulo N. */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            /* Register the string in text_buf[r..r+F-1] */
            insert_node(sp, r);
        }
        while (i++ < last_match_length) {
        delete_node(sp, s);
            /* After the end of text, no need to read, */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            /* but buffer may not be empty. */
            if (--len)
                insert_node(sp, r);
        }
    } while (len > 0);   /* until length of string to be processed is zero */
    if (code_buf_ptr > 1) {    /* Send remaining code. */
        for (i = 0; i < code_buf_ptr; i++)
            if (dst < dstend)
                *dst++ = code_buf[i];
            else
                goto finish;
    }
    result = dst;
finish:
    if (sp) free(sp);
    return result;
}

#define MAX_COLORS 256

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pixel_t;

static uint8_t clut_size = 0;
static pixel_t clut[MAX_COLORS];

static uint8_t 
lookup_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t i;

    for (i = 0; i <= clut_size; i++) {
        if (clut[i].r == r &&
            clut[i].g == g &&
            clut[i].b == b)
            return i;
    }
    if (clut_size == MAX_COLORS - 1) {
        printf("Image must have no more than 256 unique pixel colors\n");
        exit(1);
    }
    clut_size++;
    clut[clut_size].r = r;
    clut[clut_size].g = g;
    clut[clut_size].b = b;
//    printf("added color %d:%d:%d\n", r, g, b);
    return clut_size;
}

 #include "/sandbox//xnu/pexpert/pexpert/GearImage.h"
 
int main (int argc, char * argv[])
{
    int i, size = 0;
    uint8_t *unpacked = NULL;
    uint8_t *packed = NULL;
    uint8_t *lastbyte;
    uint8_t *byte;
    uint8_t color;
    char *tag;
    bool mono;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    if (argc <= 1) {
        printf("usage: PackImage <filename.png>\n");
        return 1;
    }

    NSString* filePath = [NSString stringWithUTF8String:argv[1]];
	
    NSData* fileData = [NSData dataWithContentsOfFile:filePath];
    NSBitmapImageRep* bitmapImageRep = [[NSBitmapImageRep alloc] initWithData:fileData];

    NSSize imageSize = [bitmapImageRep size];

    // Get a file name without .png extension
    tag = strstr(argv[1], ".png");
    if (tag != NULL) {
        *tag = '\0';
        tag = (char *)argv[1];
    } else {
        printf("PackImage only supports .png images \n");
        return 1;
    }
	mono = true;
	
    unpacked = malloc((int)imageSize.width * (int)imageSize.height);
    packed = malloc(1000000);

    if (unpacked == NULL || packed == NULL) {
        printf("out of memory\n");
        return 1;
    }

    bzero(clut, sizeof(clut));
    clut[0].r = 0xff;
    clut[0].g = 0xff;
    clut[0].b = 0xff;

    printf("/*\n");
    printf(" * This file was generated using PackImage utility.\n");
    printf(" * Please don't modify it.\n");
    printf(" */\n");
 
    for (int y=0; y<imageSize.height; y++) {
        for (int x=0; x<imageSize.width; x++) {
            NSUInteger pixel[4] = {};
            [bitmapImageRep getPixel:pixel atX:x y:y];

			if (0xff != (uint8_t)pixel[3]) {
				fprintf(stderr, "has alpha\n");
				exit(1);
			}
			if (1)
			{
				color = pixel[0];
				if ((color != (uint8_t)pixel[1]) || (color != (uint8_t)pixel[2])) {
					fprintf(stderr, "has colors\n");
					exit(1);
				}		
			}
			else color = lookup_color((uint8_t)pixel[0],
                                 (uint8_t)pixel[1],
                                 (uint8_t)pixel[2]);

            unpacked[size++] = color;

			
			color = gGearPict[y*(int)imageSize.width + x];
			color = (0xbf * color + 0xff) >> 8;
			pixel[0] = pixel[1] = pixel[2] = color;
			[bitmapImageRep setPixel:pixel atX:x y:y];
        }
    }

	fileData = [bitmapImageRep representationUsingType:NSPNGFileType properties:nil];
    filePath = [NSString stringWithUTF8String:"/tmp/x.png"];
	[fileData writeToFile:filePath options:0 error:NULL];

	exit(0);

    printf("\n"); 
    printf("#define k%sWidth        (%d)\n", tag, (int)imageSize.width);
    printf("#define k%sHeight       (%d)\n", tag, (int)imageSize.height);
    printf("#define k%sUnpackedSize (%d)\n", tag, size);
    if (!mono) printf("#define k%sClutSize     (%d)\n", tag, clut_size);
    
    printf("\nconst uint8_t g%sPacked[] = {\n    ", tag);

    lastbyte = compress_lzss(packed, 1000000, unpacked, size);

    // Dump LZSS-compressed image data
    for (byte = packed, i = 0; byte < lastbyte; byte++, i++) {
        if ((i > 0) && (i % 16) == 0)
            printf("\n    ");
        printf("0x%02x,", *byte);
   }
 
    printf("\n};\n\n");

	if (!mono) {
		// Dump clut table
		printf("const uint8_t g%sClut[ 256 * 3 ] =\n{\n    ", tag);
		for (i = 0; i < 256; i++) {
		   if ((i > 0) && (i % 4) == 0)
			   printf("\n    ");
		   printf("0x%02x,0x%02x,0x%02x, ", clut[i].r, clut[i].g, clut[i].b);
		}
		printf("\n};\n");
	}    
    [pool drain];
    return 0;
}
