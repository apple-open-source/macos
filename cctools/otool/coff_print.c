#include <stdio.h>
#include <string.h>
#include "mach-o/loader.h"
#include "stuff/bytesex.h"
#include "ofile_print.h"
#include "coff/base_relocs.h"
#include "coff/bytesex.h"

void
print_coff_reloc_section(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
uint32_t filetype,
enum byte_sex object_byte_sex,
char *object_addr,
uint32_t object_size,
enum bool verbose)
{
    char *sect, *p;
    uint64_t sect_addr, sect_size, size;
    int64_t left;
    struct relocation_info *sect_relocs;
    uint32_t sect_nrelocs, sect_flags, block_size;
    enum byte_sex host_byte_sex;
    enum bool swapped;
    struct base_relocation_block_header h;
    struct base_relocation_entry b;

	printf("Contents of (__RELOC,__reloc) section\n");
	if(get_sect_info("__RELOC", "__reloc", load_commands,
	    ncmds, sizeofcmds, filetype, object_byte_sex,
	    object_addr, object_size, &sect, &sect_size, &sect_addr,
	    &sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

	    host_byte_sex = get_host_byte_sex();
	    swapped = host_byte_sex != object_byte_sex;

	    p = sect;
	    left = sect_size;
	    while(left > 0){
		memset(&h, '\0', sizeof(struct base_relocation_block_header));
		size = left < sizeof(struct base_relocation_block_header) ?
		       left : sizeof(struct base_relocation_block_header);
		memcpy(&h, p, size);
		if(swapped)
		    swap_base_relocation_block_header(&h, host_byte_sex);
		printf("Page RVA   0x%08x\n", h.page_rva);
		printf("Block Size 0x%08x\n", h.block_size);

		p += sizeof(struct base_relocation_block_header);
		left -= sizeof(struct base_relocation_block_header);
		block_size = sizeof(struct base_relocation_block_header);
		while(block_size < h.block_size && left > 0){
		    memset(&b, '\0', sizeof(struct base_relocation_entry));
		    size = left < sizeof(struct base_relocation_entry) ?
			   left : sizeof(struct base_relocation_entry);
		    memcpy(&b, p, size);
		    if(swapped)
			swap_base_relocation_entry(&b, 1, host_byte_sex);
		    switch(b.type){
		    case IMAGE_REL_BASED_ABSOLUTE:
			printf("    Type   IMAGE_REL_BASED_ABSOLUTE\n");
			break;
		    case IMAGE_REL_BASED_HIGHLOW:
			printf("    Type   IMAGE_REL_BASED_HIGHLOW\n");
			break;
		    case IMAGE_REL_BASED_DIR64:
			printf("    Type   IMAGE_REL_BASED_DIR64\n");
			break;
		    default:
			printf("    Type   %u\n", b.type);
			break;
		    }
		    printf("    Offset 0x%0x\n", b.offset);

		    p += sizeof(struct base_relocation_entry);
		    left -= sizeof(struct base_relocation_entry);
		    block_size += sizeof(struct base_relocation_entry);
		}
	    }
	}
}
