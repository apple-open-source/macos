/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vloaduc.c,v 1.12 2000/04/07 03:57:51 tsi Exp $ */
/*
 * includes
 */

#include "rendition.h"
#include "v1kregs.h"
#include "v1krisc.h"
#include "vloaduc.h"
#include "vos.h"
#include "elf.h"


/*
 * defines 
 */

#ifdef X_LITTLE_ENDIAN

/* maybe swap word */
#define SW32(x) lswapl(x)
#define SW16(x) lswaps(x)
#else /* BIG_ENDIAN */
#define SW32(x) (x)
#define SW16(x) (x)
#endif /* #ifdef LITTLE_ENDIAN */



/*
 * local function prototypes 
 */
static void loadSection2board(ScrnInfoPtr pScreenInfo, int fd,
				Elf32_Shdr *shdr);
static void loadSegment2board(ScrnInfoPtr pScreenInfo, int fd,
				Elf32_Phdr *phdr);
static int seek_and_read_hdr(int fd, void *ptr, long int offset,
			     int size, int cnt);
static void mmve(ScrnInfoPtr pScreenInfo, vu32 size, vu8 *data, vu32 phys_addr);



/*
 * functions
 */

/* 
 * int verite_load_ucfile(ScrnInfoPtr pScreenInfo, char *file_name)
 *
 * Loads verite elf file microcode file in |name| onto the board.
 * NOTE: Assumes the ucode loader is already running on the board!
 * 
 * Returns the program's entry point, on error -1;
 */
int
verite_load_ucfile(ScrnInfoPtr pScreenInfo, char *file_name)
{
  /* renditionPtr pRendition = RENDITIONPTR(pScreenInfo); */

  int num;
  int sz;
  int fd;
  Elf32_Phdr *pphdr, *orig_pphdr=NULL;
  Elf32_Shdr *pshdr, *orig_pshdr=NULL;
  Elf32_Ehdr ehdr ;

#ifdef DEBUG
  ErrorF("RENDITION: Loading microcode %s\n", file_name); 
#endif

  /* Stop the RISC if it happends to run */
  v1k_stop (pScreenInfo);

  /* open file and read ELF-header */
  if (-1 == (fd=open(file_name, O_RDONLY, 0))) {
    ErrorF("RENDITION: Cannot open microcode %s\n", file_name); 
    return -1;
  }

  if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
    ErrorF("RENDITION: Cannot read microcode header %s\n", file_name); 
    return -1;
  }
  if (0 != strncmp((char *)&ehdr.e_ident[1], "ELF", 3)) {
    ErrorF("RENDITION: Microcode header in %s is corrupt\n", file_name); 
    return -1;
  }

  /* read in the program header(s) */
  sz=SW16(ehdr.e_phentsize);
  num=SW16(ehdr.e_phnum);
  if (0!=sz && 0!=num) {
	orig_pphdr=pphdr=(Elf32_Phdr *)xalloc(sz*num);
	if (!pphdr) {
	  ErrorF("RENDITION: Cannot allocate global memory (1)\n"); 
	  close(fd);
	  return -1;
	}

	if (seek_and_read_hdr(fd, pphdr, SW32(ehdr.e_phoff), sz, num)) {
	  ErrorF("RENDITION: Error reading microcode (1)\n");
	  close(fd);
	  return -1;
	}

	orig_pshdr=pshdr=(Elf32_Shdr *)0;
  }
  else {
	orig_pphdr=pphdr=(Elf32_Phdr *)0;

    /* read in the section header(s) */
    sz=SW16(ehdr.e_shentsize);
    num=SW16(ehdr.e_shnum);
    if (0!=sz && 0!=num) {
      orig_pshdr=pshdr=(Elf32_Shdr *)xalloc(sz*num);
      if (!pshdr) {
        ErrorF("RENDITION: Cannot allocate global memory (2)\n"); 
        close(fd);
        return -1;
      }

      if (seek_and_read_hdr(fd, pshdr, SW32(ehdr.e_shoff), sz, num)) {
        ErrorF("RENDITION: Error reading microcode (2)\n");
        close(fd);
        return -1;
      }
    }
    else
      pshdr=(Elf32_Shdr *)0;
  }

  if (pphdr) {
    do {
      if (SW32(pphdr->p_type) == PT_LOAD) 
        loadSegment2board(pScreenInfo, fd, pphdr);
        pphdr=(Elf32_Phdr *)(((char *)pphdr)+sz);
      } while (--num);
      xfree(orig_pphdr);
  }
  else {
    do {
      if (SW32(pshdr->sh_size) && (SW32(pshdr->sh_flags) & SHF_ALLOC)
          && ((SW32(pshdr->sh_type)==SHT_PROGBITS) 
               || (SW32(pshdr->sh_type)==SHT_NOBITS))) 
        loadSection2board(pScreenInfo, fd, pshdr);
	  pshdr=(Elf32_Shdr *)(((char *)pshdr)+sz);
	} while (--num) ;
	xfree(orig_pshdr);
  }
  close(fd);

  return SW32(ehdr.e_entry);
}



/*
 * local functions
 */

static void
loadSection2board(ScrnInfoPtr pScreenInfo, int fd, Elf32_Shdr *shdr)
{
  /*  renditionPtr pRendition = RENDITIONPTR(pScreenInfo); */
   ErrorF("vlib: loadSection2board not implemented yet!\n");
}



static void
loadSegment2board(ScrnInfoPtr pScreenInfo, int fd, Elf32_Phdr *phdr)
{
  /*  renditionPtr pRendition = RENDITIONPTR(pScreenInfo); */
  vu8 *data;
  vu32 offset=SW32(phdr->p_offset);
  vu32 size=SW32(phdr->p_filesz);
  vu32 physAddr=SW32(phdr->p_paddr);

  if (lseek(fd, offset, SEEK_SET) != offset) {
	ErrorF("RENDITION: Failure in loadSegmentToBoard, offset %lx\n", offset);
    return;
  }

  data=(vu8 *)xalloc(size);
  if (NULL == data){
	ErrorF("RENDITION: GlobalAllocPtr couldn't allocate %x bytes", size);
	return;
  }

  if (read(fd, data, size) != size){
	ErrorF("RENDITION: verite_readfile Failure, couldn't read %x bytes ", size);
	return;
  }

  mmve(pScreenInfo, size, data, physAddr);

  xfree(data);
}



static int
seek_and_read_hdr(int fd, void *ptr, long int offset, int size, 
                             int cnt)
{
  if (lseek(fd, offset, SEEK_SET) != offset)
    return 1 ;

  if (size*cnt != read(fd, ptr, size*cnt))
    return 2 ;

  return 0 ;
}



static void
mmve(ScrnInfoPtr pScreenInfo, vu32 size, vu8 *data, vu32 phys_addr)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  vu8 memend;
  vu32 *dataout;
  vu8 *vmb=pRendition->board.vmem_base;

  /* swap bytes 3<>0, 2<>1 */
  memend=verite_in8(pRendition->board.io_base+MEMENDIAN);
  verite_out8(pRendition->board.io_base+MEMENDIAN, MEMENDIAN_END);

  dataout=(vu32 *)data;

  /* If RISC happends to be running, be sure it is stopped */
  v1k_stop(pScreenInfo);

  while (size > 0) {
    verite_write_memory32(vmb, phys_addr, *dataout);
        phys_addr+=4;
        dataout++;
	size-=4;
  }

  verite_out8(pRendition->board.io_base+MEMENDIAN, memend);
}

/*
 * end of file vloaduc.c
 */
