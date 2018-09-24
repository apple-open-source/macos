//
//  t_zlib.c
//  t_zlib
//
//  Created by Tal Uliel on 2/7/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define ZLIB_PADDING_SIZE (4 * 65536 + 8192) // Empirically determined alloc size for our deflateInit2 call + z_stream
typedef struct {
  
  uint8_t * buf;          // pre-allocated buffer
  size_t size;            // available bytes in BUF
  
} zlib_alloc_state;


// Alloc item*size bytes. Return 0 on failure.
// OPAQUE shall point to a zlib_alloc_state, and will be updated.
static void * zlib_malloc(void * opaque,uInt items,uInt itemSize)
{
  zlib_alloc_state * s = (zlib_alloc_state *)opaque;
  size_t size = (size_t)items*(size_t)itemSize;
  
  if ((size > 0) && (itemSize > 0) &&
      ((size < items) || (size < itemSize)) )
    return 0; //overflow
  
  // Fail if not enough space remaining in s->buf
  if (size > s->size) return 0;
  
  // Return BUF, and consume SIZE bytes
  void * ptr = s->buf;
  s->buf += size;
  s->size -= size;
  return ptr;
}

// Free PTR.
// OPAQUE shall point to a zlib_alloc_state, and will be updated.
static void zlib_free(void * opaque,void * ptr)
{
  // We don't free anything
}

static int read_parameters(int argc, const char * argv[], FILE ** f)
{
  if (argc == 1)
  {
    fprintf(stderr, "t_zlib: error, input file wasn't specified\n");
    return -1; // fail
  }
  
  for (int i = 1; i < argc;)
  {
    if ((strcmp(argv[i],"-h")==0) || (strcmp(argv[i],"-help")==0))
    {
      return 1; // need to print help
    } else
    {
      *f = fopen(argv[i], "rb");
      if (*f == 0)
      {
        fprintf(stderr,"t_zlib: failed to open %s\n", argv[i]);
        return -1; // fail
      }
      if ((i+1) != argc)
      {
        fprintf(stderr, "t_zlib: ignoring rest of the paramters after file path %s\n", argv[i]);
        return 0; // success
      }
      i++;
    }
  }
  
  return 0;
}

static void print_help()
{
  fprintf(stderr, "t_zlib help\n");
  fprintf(stderr, "t_zlib <path_to_file>\n");
  fprintf(stderr, "t_zlib will load content from a file and verify encode/decode\n");
  fprintf(stderr, "-h/-help - print help message");
}

/*!
 @abstract simple tester for zlib
 @discussion simple tester to load content from a file and verify encode/decode
 */
int main(int argc, const char * argv[])
{
  FILE * f;
  int res = read_parameters(argc, argv, &f);
  
  if (res)
  {
    print_help();
    
    // return 0 if user requested to print help menu (-h/-help)
    return (res < 0);
  }
  
  fseek(f, 0, SEEK_END);
  size_t number_of_bytes = ftell(f);
  Bytef * input_buf   = malloc(number_of_bytes); // buffer that will hold the input byts from the file
  if (input_buf == 0) { fprintf(stderr, "t_zlib: alloc failed\n"); return -1;}

  // read input from file
  fseek(f, 0, SEEK_SET);
  fread(input_buf, 1, number_of_bytes, f);

  // close the file
  fclose(f);

  // assuming 2x the input size + ZLIB_PADDING_SIZE would be enough
  Bytef * encoded_buf = malloc(2*number_of_bytes + ZLIB_PADDING_SIZE); // buffer to hold the encode result
  if (encoded_buf == 0) { fprintf(stderr, "t_zlib: alloc failed\n"); return -1;}
  
  // set stream structre for encode
  z_stream desc;
  bzero(&desc, sizeof(desc));
  
  uint8_t scratch[ZLIB_PADDING_SIZE];
  zlib_alloc_state opaque;
  opaque.buf = (uint8_t*)scratch;
  opaque.size = ZLIB_PADDING_SIZE;
  
  desc.next_in = input_buf;
  desc.avail_in = (uint32_t)number_of_bytes;
  desc.next_out = encoded_buf;
  desc.avail_out = (uint32_t)(2*number_of_bytes + ZLIB_PADDING_SIZE);
  desc.zalloc = zlib_malloc;
  desc.zfree = zlib_free;
  desc.opaque = &opaque;
  
  // encode file
  res = deflateInit2(&desc, 5, Z_DEFLATED,(-15),8,Z_DEFAULT_STRATEGY);
  
  res = deflate(&desc, Z_FINISH);
  if (res != Z_STREAM_END) { fprintf(stderr, "zlib: encode failed\n"); return -1; }

  size_t encoded_size = desc.total_out;
  deflateEnd(&desc);
  
  // realloc encoede buf to limit buffer size
  encoded_buf = realloc(encoded_buf, encoded_size);
  
  Bytef * decoded_buf = malloc(number_of_bytes); // bufert hat will hold the decode result
  if (decoded_buf == 0) { fprintf(stderr, "t_zlib: alloc failed\n"); return -1;}

  // set stream structre for encode
  bzero(&desc, sizeof(desc));
  opaque.buf = (uint8_t*)scratch;
  opaque.size = ZLIB_PADDING_SIZE;

  desc.next_in = encoded_buf;
  desc.avail_in = (uint32_t)encoded_size;
  desc.next_out = decoded_buf;
  desc.avail_out = (uint32_t)number_of_bytes;
  desc.zalloc = zlib_malloc;
  desc.zfree = zlib_free;
  desc.opaque = &opaque;
  desc.adler = (uint32_t)adler32(0, input_buf, (uint32_t)number_of_bytes);

  // decode buffer
  inflateInit2(&desc, (-15));

  res = inflate(&desc, Z_FINISH);
  if (res != Z_STREAM_END) { fprintf(stderr, "zlib: decode failed\n"); return -1; }
  
  inflateEnd(&desc);

  free(encoded_buf);

  // compare buffers
  for (size_t i = 0; i < number_of_bytes; i++)
  {
    if (input_buf[i] != decoded_buf[i])
    {
      fprintf(stderr, "t_zlib: valdiation fail, buffers differ in byte %zu\n", i);
      return -1;
    }
  }
  
  free(input_buf);
  free(decoded_buf);
  
  fprintf(stderr, "PASSED\n");
  return 0;
}
