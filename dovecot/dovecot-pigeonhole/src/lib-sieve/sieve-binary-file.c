/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"
#include "mempool.h"
#include "buffer.h"
#include "hash.h"
#include "array.h"
#include "ostream.h"
#include "eacces-error.h"	
#include "safe-mkstemp.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-extensions.h"
#include "sieve-code.h"
#include "sieve-script.h"

#include "sieve-binary-private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Macros
 */

#define SIEVE_BINARY_MAGIC              0xcafebabe
#define SIEVE_BINARY_MAGIC_OTHER_ENDIAN 0xbebafeca 

#define SIEVE_BINARY_ALIGN(offset) \
	(((offset) + 3) & ~3)
#define SIEVE_BINARY_ALIGN_PTR(ptr) \
	((void *) SIEVE_BINARY_ALIGN(((size_t) ptr)))

/*
 * Header and record structures of the binary on disk 
 */
 
struct sieve_binary_header {
	uint32_t magic;
	uint16_t version_major;
	uint16_t version_minor;
	uint32_t blocks;
};

struct sieve_binary_block_index {
	uint32_t id;
	uint32_t size;
	uint32_t offset;
	uint32_t ext_id;
};

struct sieve_binary_block_header {
	uint32_t id; 
	uint32_t size;
};

/* 
 * Saving the binary to a file. 
 */

static inline bool _save_skip
(struct sieve_binary *sbin, struct ostream *stream, size_t size)
{	
	if ( (o_stream_seek(stream, stream->offset + size)) <= 0 ) {
		sieve_sys_error(sbin->svinst,
			"binary save: failed to skip output stream "
			"to position %"PRIuUOFF_T": %s", stream->offset + size,
			strerror(stream->stream_errno));
		return FALSE;
	}

	return TRUE;
}

static inline bool _save_skip_aligned
(struct sieve_binary *sbin, struct ostream *stream, size_t size, 
	uoff_t *offset)
{
	uoff_t aligned_offset = SIEVE_BINARY_ALIGN(stream->offset);
	
	if ( (o_stream_seek(stream, aligned_offset + size)) <= 0 ) {
		sieve_sys_error(sbin->svinst, "binary save: failed to skip output stream "
			"to position %"PRIuUOFF_T": %s", aligned_offset + size,
			strerror(stream->stream_errno));
		return FALSE;
	}
		
	if ( offset != NULL )
		*offset = aligned_offset;
		
	return TRUE;
}

/* FIXME: Is this even necessary for a file? */
static bool _save_full
(struct sieve_binary *sbin, struct ostream *stream, const void *data, size_t size)
{
	size_t bytes_left = size;
	const void *pdata = data;
	
	while ( bytes_left > 0 ) {
		ssize_t ret;
		
		if ( (ret=o_stream_send(stream, pdata, bytes_left)) <= 0 ) {
			sieve_sys_error(sbin->svinst,
				"binary save: failed to write %"PRIuSIZE_T" bytes "
				"to output stream: %s", bytes_left, strerror(stream->stream_errno));
			return FALSE;
		}
			
		pdata = PTR_OFFSET(pdata, ret);
		bytes_left -= ret;
	}	
	
	return TRUE;
}

static bool _save_aligned
(struct sieve_binary *sbin, struct ostream *stream, const void *data,
	size_t size, uoff_t *offset)
{	
	uoff_t aligned_offset = SIEVE_BINARY_ALIGN(stream->offset);

	o_stream_cork(stream);
	
	/* Align the data by adding zeroes to the output stream */
	if ( stream->offset < aligned_offset ) {
		if ( !_save_skip(sbin, stream, aligned_offset - stream->offset) ) 
			return FALSE;
	}
	
	if ( !_save_full(sbin, stream, data, size) )
		return FALSE;
	
	o_stream_uncork(stream); 

	if ( offset != NULL )
		*offset = aligned_offset;

	return TRUE;
} 

static bool _save_block
(struct sieve_binary *sbin, struct ostream *stream, unsigned int id)
{
	struct sieve_binary_block_header block_header;
	struct sieve_binary_block *block;
	const void *data;
	size_t size;
		
	block = sieve_binary_block_get(sbin, id);
	if ( block == NULL )
		return FALSE;
		
	data = buffer_get_data(block->data, &size);
	
	block_header.id = id;
	block_header.size = size;
	
	if ( !_save_aligned(sbin, stream, &block_header,
		sizeof(block_header), &block->offset) )
		return FALSE;
	
	return _save_aligned(sbin, stream, data, size, NULL);
}

static bool _save_block_index_record
(struct sieve_binary *sbin, struct ostream *stream, unsigned int id)
{
	struct sieve_binary_block *block;
	struct sieve_binary_block_index header;
	
	block = sieve_binary_block_get(sbin, id);
	if ( block == NULL )
		return FALSE;
	
	header.id = id;
	header.size = buffer_get_used_size(block->data);
	header.ext_id = block->ext_index;
	header.offset = block->offset;
	
	if ( !_save_full(sbin, stream, &header, sizeof(header)) ) {
		sieve_sys_error(sbin->svinst,
			"binary save: failed to save block index header %d", id);
		return FALSE;
	}
	
	return TRUE;
}

static bool _sieve_binary_save
(struct sieve_binary *sbin, struct ostream *stream)
{
	struct sieve_binary_header header;
	struct sieve_binary_extension_reg *const *regs;
	struct sieve_binary_block *ext_block;
	unsigned int ext_count, blk_count, i;
	uoff_t block_index;
	
	blk_count = sieve_binary_block_count(sbin);
	
	/* Signal all extensions to finish generating their blocks */
	
	regs = array_get(&sbin->extensions, &ext_count);	
	for ( i = 0; i < ext_count; i++ ) {
		const struct sieve_binary_extension *binext = regs[i]->binext;
		
		if ( binext != NULL && binext->binary_save != NULL )
			binext->binary_save(regs[i]->extension, sbin, regs[i]->context);
	}
		
	/* Create header */
	
	header.magic = SIEVE_BINARY_MAGIC;
	header.version_major = SIEVE_BINARY_VERSION_MAJOR;
	header.version_minor = SIEVE_BINARY_VERSION_MINOR;
	header.blocks = blk_count;

	if ( !_save_aligned(sbin, stream, &header, sizeof(header), NULL) ) {
		sieve_sys_error(sbin->svinst, "binary save: failed to save header");
		return FALSE;
	} 
	
	/* Skip block index for now */
	
	if ( !_save_skip_aligned(sbin, stream, 
		sizeof(struct sieve_binary_block_index) * blk_count, &block_index) )
		return FALSE;
	
	/* Create block containing all used extensions 
	 *   FIXME: Per-extension this should also store binary version numbers.
	 */
	ext_block = sieve_binary_block_get(sbin, SBIN_SYSBLOCK_EXTENSIONS);
	i_assert( ext_block != NULL );
		
	ext_count = array_count(&sbin->linked_extensions);
	sieve_binary_emit_unsigned(ext_block, ext_count);
	
	for ( i = 0; i < ext_count; i++ ) {
		struct sieve_binary_extension_reg * const *ext
			= array_idx(&sbin->linked_extensions, i);
		
		sieve_binary_emit_cstring
			(ext_block, sieve_extension_name((*ext)->extension));
		sieve_binary_emit_unsigned(ext_block, (*ext)->block_id);
	}
		
	/* Save all blocks into the binary */
	
	for ( i = 0; i < blk_count; i++ ) {
		if ( !_save_block(sbin, stream, i) ) 
			return FALSE;
	}
	
	/* Create the block index */
	o_stream_seek(stream, block_index);
	for ( i = 0; i < blk_count; i++ ) {
		if ( !_save_block_index_record(sbin, stream, i) ) 
			return FALSE;
	}

	return TRUE;
} 

int sieve_binary_save
(struct sieve_binary *sbin, const char *path, bool update, 
	enum sieve_error *error_r)
{
	int result, fd;
	string_t *temp_path;
	struct ostream *stream;
	mode_t save_mode =
		sbin->script == NULL ? 0600 : sieve_script_permissions(sbin->script);

	if ( error_r != NULL )
		*error_r = SIEVE_ERROR_NONE;
	
	/* Use default path if none is specified */
	if ( path == NULL ) {
		if ( sbin->script == NULL ) {
			sieve_sys_error(sbin->svinst, 
				"binary save: cannot determine default path "
				"with missing script object");
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NOT_POSSIBLE;
			return -1;
		}
		path = sieve_script_binpath(sbin->script);
	}

	/* Check whether saving is necessary */
	if ( !update && sbin->path != NULL && strcmp(sbin->path, path) == 0 ) {
		if ( sbin->svinst->debug ) {
			sieve_sys_debug(sbin->svinst, "binary save: not saving binary %s, "
				"because it is already stored", path);
		}		
		return 0;
	}

	/* Open it as temp file first, as not to overwrite an existing just yet */
	temp_path = t_str_new(256);
	str_append(temp_path, path);
	str_append_c(temp_path, '.');
	fd = safe_mkstemp_hostpid(temp_path, save_mode, (uid_t)-1, (gid_t)-1);
	if ( fd < 0 ) {
		if ( errno == EACCES ) {
			sieve_sys_error(sbin->svinst,
				"binary save: failed to create temporary file: %s",
				eacces_error_get_creating("open", str_c(temp_path)));
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NO_PERM;
		} else {
			sieve_sys_error(sbin->svinst,
				"binary save: failed to create temporary file: open(%s) failed: %m",
				str_c(temp_path));
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_TEMP_FAIL;
		}
		return -1;
	}

	/* Save binary */
	result = 1;
	stream = o_stream_create_fd(fd, 0, FALSE);
	if ( !_sieve_binary_save(sbin, stream) ) {
		result = -1;
		if ( error_r != NULL )
			*error_r = SIEVE_ERROR_TEMP_FAIL;
	}
	o_stream_destroy(&stream);

	/* Close saved binary */ 
	if ( close(fd) < 0 ) {
		sieve_sys_error(sbin->svinst,
			"binary save: failed to close temporary file: "
			"close(fd=%s) failed: %m", str_c(temp_path));
	}

	/* Replace any original binary atomically */
	if ( result && (rename(str_c(temp_path), path) < 0) ) {
		if ( errno == EACCES ) {
			sieve_sys_error(sbin->svinst, "binary save: failed to save binary: %s", 
				eacces_error_get_creating("rename", path));
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NO_PERM;
		} else { 		
			sieve_sys_error(sbin->svinst, "binary save: failed to save binary: "
				"rename(%s, %s) failed: %m", str_c(temp_path), path);
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_TEMP_FAIL;
		}
		result = -1;
	}

	if ( result < 0 ) {
		/* Get rid of temp output (if any) */
		if ( unlink(str_c(temp_path)) < 0 && errno != ENOENT ) {
			sieve_sys_error(sbin->svinst, 
				"binary save: failed to clean up after error: unlink(%s) failed: %m",
				str_c(temp_path));
		}
	} else {
		if ( sbin->path == NULL ) {
			sbin->path = p_strdup(sbin->pool, path);
		}
	}
	
	return result;
}

/* 
 * Binary file management 
 */

bool sieve_binary_file_open
(struct sieve_binary_file *file, 
	struct sieve_instance *svinst, const char *path, enum sieve_error *error_r)
{
	int fd;
	bool result = TRUE;
	struct stat st;

	if ( error_r != NULL )
		*error_r = SIEVE_ERROR_NONE;
	
	if ( (fd=open(path, O_RDONLY)) < 0 ) {
		switch ( errno ) {
		case ENOENT:
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NOT_FOUND;
			break;
		case EACCES:
			sieve_sys_error(svinst, "binary open: failed to open: %s", 
				eacces_error_get("open", path));
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NO_PERM;
			break;
		default:
			sieve_sys_error(svinst, "binary open: failed to open: "
				"open(%s) failed: %m", path);
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_TEMP_FAIL;
			break;
		}
		return FALSE;
	}

	if ( fstat(fd, &st) < 0 ) {
		if ( errno != ENOENT ) {
			sieve_sys_error(svinst,
				"binary open: fstat(fd=%s) failed: %m", path);
		}
		result = FALSE;
	}

	if ( result && !S_ISREG(st.st_mode) ) {
		sieve_sys_error(svinst, 
			"binary open: %s is not a regular file", path);
		result = FALSE;		
	}
	
	if ( !result )	{
		if ( close(fd) < 0 ) {
			sieve_sys_error(svinst,
				"binary open: close(fd=%s) failed after error: %m", path);
		}
		return FALSE;
	}

	file->svinst = svinst;
	file->fd = fd;
	file->st = st;

	return TRUE;
}
	
void sieve_binary_file_close(struct sieve_binary_file **file)
{
	if ( (*file)->fd != -1 ) {
		if ( close((*file)->fd) < 0 ) {
			sieve_sys_error((*file)->svinst, 
				"binary close: failed to close: close(fd=%s) failed: %m",
				(*file)->path);
		}
	}

	pool_unref(&(*file)->pool);
	
	*file = NULL;
}

#if 0 /* file_memory is currently unused */

/* File loaded/mapped to memory */

struct _file_memory {
	struct sieve_binary_file binfile;

	/* Pointer to the binary in memory */
	const void *memory;
	off_t memory_size;
};

static const void *_file_memory_load_data
	(struct sieve_binary_file *file, off_t *offset, size_t size)
{	
	struct _file_memory *fmem = (struct _file_memory *) file;

	*offset = SIEVE_BINARY_ALIGN(*offset);

	if ( (*offset) + size <= fmem->memory_size ) {
		const void *data = PTR_OFFSET(fmem->memory, *offset);
		*offset += size;
		file->offset = *offset;
		
		return data;
	}
		
	return NULL;
}

static buffer_t *_file_memory_load_buffer
	(struct sieve_binary_file *file, off_t *offset, size_t size)
{	
	struct _file_memory *fmem = (struct _file_memory *) file;

	*offset = SIEVE_BINARY_ALIGN(*offset);

	if ( (*offset) + size <= fmem->memory_size ) {
		const void *data = PTR_OFFSET(fmem->memory, *offset);
		*offset += size;
		file->offset = *offset;
		
		return buffer_create_const_data(file->pool, data, size);
	}
	
	return NULL;
}

static bool _file_memory_load(struct sieve_binary_file *file)
{
	struct _file_memory *fmem = (struct _file_memory *) file;
	int ret;
	size_t size;
	void *indata;
		
	i_assert(file->fd > 0);
		
	/* Allocate memory buffer
	 */
	indata = p_malloc(file->pool, file->st.st_size);
	size = file->st.st_size; 
	
	file->offset = 0; 
	fmem->memory = indata;
	fmem->memory_size = file->st.st_size;

	/* Return to beginning of the file */
	if ( lseek(file->fd, 0, SEEK_SET) == (off_t) -1 ) {
		sieve_sys_error("failed to seek() in binary %s: %m", file->path);
		return FALSE;
	}	

	/* Read the whole file into memory */
	while (size > 0) {
		if ( (ret=read(file->fd, indata, size)) <= 0 ) {
			sieve_sys_error("failed to read from binary %s: %m", file->path);
			break;
		}
		
		indata = PTR_OFFSET(indata, ret);
		size -= ret;
	}	

	if ( size != 0 ) {
		/* Failed to read the whole file */
		return FALSE;
	}
	
	return TRUE;
}

static struct sieve_binary_file *_file_memory_open(const char *path)
{
	pool_t pool;
	struct _file_memory *file;
	
	pool = pool_alloconly_create("sieve_binary_file_memory", 1024);
	file = p_new(pool, struct _file_memory, 1);
	file->binfile.pool = pool;
	file->binfile.path = p_strdup(pool, path);
	file->binfile.load = _file_memory_load;
	file->binfile.load_data = _file_memory_load_data;
	file->binfile.load_buffer = _file_memory_load_buffer;
	
	if ( !sieve_binary_file_open(&file->binfile, path) ) {
		pool_unref(&pool);
		return NULL;
	}

	return &file->binfile;
}

#endif /* file_memory is currently unused */

/* File open in lazy mode (only read what is needed into memory) */

static bool _file_lazy_read
(struct sieve_binary_file *file, off_t *offset, void *buffer, size_t size)
{
	struct sieve_instance *svinst = file->svinst;
	int ret;
	void *indata = buffer;
	size_t insize = size;
	
	*offset = SIEVE_BINARY_ALIGN(*offset);
	
	/* Seek to the correct position */ 
	if ( *offset != file->offset && 
		lseek(file->fd, *offset, SEEK_SET) == (off_t) -1 ) {
		sieve_sys_error(svinst, "binary read:"
			"failed to seek(fd, %lld, SEEK_SET) in binary %s: %m", 
			(long long) *offset, file->path);
		return FALSE;
	}	

	/* Read record into memory */
	while (insize > 0) {
		if ( (ret=read(file->fd, indata, insize)) <= 0 ) {
			if ( ret == 0 ) 
				sieve_sys_error(svinst, 
					"binary read: binary %s is truncated (more data expected)", 
					file->path);
			else
				sieve_sys_error(svinst,
					"binary read: failed to read from binary %s: %m", file->path);
			break;
		}
		
		indata = PTR_OFFSET(indata, ret);
		insize -= ret;
	}	

	if ( insize != 0 ) {
		/* Failed to read the whole requested record */
		return FALSE;
	}
	
	*offset += size;
	file->offset = *offset;

	return TRUE;
}

static const void *_file_lazy_load_data
(struct sieve_binary_file *file, off_t *offset, size_t size)
{	
	void *data = t_malloc(size);

	if ( _file_lazy_read(file, offset, data, size) ) {
		return data;
	}
	
	return NULL;
}

static buffer_t *_file_lazy_load_buffer
(struct sieve_binary_file *file, off_t *offset, size_t size)
{			
	buffer_t *buffer = buffer_create_dynamic(file->pool, size);
	
	if ( _file_lazy_read
		(file, offset, buffer_get_space_unsafe(buffer, 0, size), size) ) {
		return buffer;
	}
	
	return NULL;
}

static struct sieve_binary_file *_file_lazy_open
(struct sieve_instance *svinst, const char *path, enum sieve_error *error_r)
{
	pool_t pool;
	struct sieve_binary_file *file;
	
	pool = pool_alloconly_create("sieve_binary_file_lazy", 4096);
	file = p_new(pool, struct sieve_binary_file, 1);
	file->pool = pool;
	file->path = p_strdup(pool, path);
	file->load_data = _file_lazy_load_data;
	file->load_buffer = _file_lazy_load_buffer;
	
	if ( !sieve_binary_file_open(file, svinst, path, error_r) ) {
		pool_unref(&pool);
		return NULL;
	}

	return file;
}

/* 
 * Load binary from a file
 */

#define LOAD_HEADER(sbin, offset, header) \
	(header *) sbin->file->load_data(sbin->file, offset, sizeof(header))

bool sieve_binary_load_block
(struct sieve_binary_block *sblock)
{
	struct sieve_binary *sbin = sblock->sbin;
	unsigned int id = sblock->id;
	off_t offset = sblock->offset;
	const struct sieve_binary_block_header *header = 
		LOAD_HEADER(sbin, &offset, const struct sieve_binary_block_header);
		
	if ( header == NULL ) {
		sieve_sys_error(sbin->svinst,
			"binary load: binary %s is corrupt: "
			"failed to read header of block %d", sbin->path, id);
		return FALSE;
	}
	
	if ( header->id != id ) {
		sieve_sys_error(sbin->svinst,
			"binary load: binary %s is corrupt: "
			"header of block %d has non-matching id %d",
			sbin->path, id, header->id);
		return FALSE;
	}
	
	sblock->data = sbin->file->load_buffer(sbin->file, &offset, header->size);
	if ( sblock->data == NULL ) {
		sieve_sys_error(sbin->svinst,
			"binary load: failed to read block %d of binary %s (size=%d)",
			id, sbin->path, header->size);
		return FALSE;
	}
		
	return TRUE;
}

static bool _read_block_index_record
(struct sieve_binary *sbin, off_t *offset, unsigned int id)
{
	const struct sieve_binary_block_index *record = 
		LOAD_HEADER(sbin, offset, const struct sieve_binary_block_index);
	struct sieve_binary_block *block;
	
	if ( record == NULL ) {
		sieve_sys_error(sbin->svinst,
			"binary open: binary %s is corrupt: "
			"failed to load block index record %d", sbin->path, id);
		return FALSE;
	}
	
	if ( record->id != id ) {
		sieve_sys_error(sbin->svinst,
			"binary open: binary %s is corrupt: "
			"block index record %d has unexpected id %d", sbin->path, id, record->id);
		return FALSE;
	}
	
	block = sieve_binary_block_create_id(sbin, id);
	block->ext_index = record->ext_id;
	block->offset = record->offset;
	
	return TRUE;
}

static bool _read_extensions(struct sieve_binary_block *sblock)
{
	struct sieve_binary *sbin = sblock->sbin;
	sieve_size_t offset = 0;
	unsigned int i, count;
	bool result = TRUE;
	
	if ( !sieve_binary_read_unsigned(sblock, &offset, &count) )
		return FALSE;
	
	for ( i = 0; result && i < count; i++ ) {
		T_BEGIN {
			string_t *extension;
			const struct sieve_extension *ext;
			
			if ( sieve_binary_read_string(sblock, &offset, &extension) ) { 
				ext = sieve_extension_get_by_name(sbin->svinst, str_c(extension));	
			
				if ( ext == NULL ) { 
					sieve_sys_error(sbin->svinst,
						"binary open: binary %s requires unknown extension '%s'", 
						sbin->path, str_sanitize(str_c(extension), 128));
					result = FALSE;					
				} else {
					struct sieve_binary_extension_reg *ereg = NULL;
					
					(void) sieve_binary_extension_register(sbin, ext, &ereg);
					if ( !sieve_binary_read_unsigned(sblock, &offset, &ereg->block_id) )
						result = FALSE;
				}
			}	else
				result = FALSE;
		} T_END;
	}		
		
	return result;
}

static bool _sieve_binary_open(struct sieve_binary *sbin)
{
	bool result = TRUE;
	off_t offset = 0;
	const struct sieve_binary_header *header;
	struct sieve_binary_block *ext_block;
	unsigned int i, blk_count;
	
	/* Verify header */
	
	T_BEGIN {
		header = LOAD_HEADER(sbin, &offset, const struct sieve_binary_header);
		/* Check header presence */
		if ( header == NULL ) {
			sieve_sys_error(sbin->svinst,
				"binary_open: file %s is not large enough to contain the header.",
				sbin->path);
			result = FALSE;

		/* Check header validity */
		} else if ( header->magic != SIEVE_BINARY_MAGIC ) {
			if ( header->magic != SIEVE_BINARY_MAGIC_OTHER_ENDIAN ) 
				sieve_sys_error(sbin->svinst, 
					"binary_open: binary %s has corrupted header "
					"(0x%08x) or it is not a Sieve binary", sbin->path, header->magic);
			else if ( sbin->svinst->debug )
				sieve_sys_debug(sbin->svinst,
					"binary open: binary %s stored with in different endian format "
					"(automatically fixed when re-compiled)",
					sbin->path);
			result = FALSE;

		/* Check binary version */
		} else if ( result && (
		  header->version_major != SIEVE_BINARY_VERSION_MAJOR || 
			header->version_minor != SIEVE_BINARY_VERSION_MINOR ) ) {

			/* Binary is of different version. Caller will have to recompile */
			
			if ( sbin->svinst->debug ) {
				sieve_sys_debug(sbin->svinst,
					"binary open: binary %s stored with different binary version %d.%d "
					"(!= %d.%d; automatically fixed when re-compiled)", sbin->path,
					(int) header->version_major, header->version_minor,
					SIEVE_BINARY_VERSION_MAJOR, SIEVE_BINARY_VERSION_MINOR);
			}
			result = FALSE;

		/* Check block content */
		} else if ( result && header->blocks == 0 ) {
			sieve_sys_error(sbin->svinst,
				"binary open: binary %s is corrupt: it contains no blocks",
				sbin->path);
			result = FALSE; 

		/* Valid */
		} else {
			blk_count = header->blocks;
		}
	} T_END;
	
	if ( !result ) return FALSE;
	
	/* Load block index */
	
	for ( i = 0; i < blk_count && result; i++ ) {	
		T_BEGIN {
			if ( !_read_block_index_record(sbin, &offset, i) ) {
				result = FALSE;
			}
		} T_END;
	}
	
	if ( !result ) return FALSE;
	
	/* Load extensions used by this binary */
	
	T_BEGIN {
		ext_block = sieve_binary_block_get(sbin, SBIN_SYSBLOCK_EXTENSIONS);
		if ( ext_block == NULL ) {
			result = FALSE;
		} else {
			if ( !_read_extensions(ext_block) ) {
				sieve_sys_error(sbin->svinst,
					"binary open: binary %s is corrupt: failed to load extension block", 
					sbin->path);
				result = FALSE;
			}
		}
	} T_END;
		
	return result;
}

struct sieve_binary *sieve_binary_open
(struct sieve_instance *svinst, const char *path, struct sieve_script *script,
	enum sieve_error *error_r)
{
	struct sieve_binary_extension_reg *const *regs;
	unsigned int ext_count, i;
	struct sieve_binary *sbin;
	struct sieve_binary_file *file;
	
	i_assert( script == NULL || sieve_script_svinst(script) == svinst );
	
	//file = _file_memory_open(path);	
	if ( (file=_file_lazy_open(svinst, path, error_r)) == NULL ) 
		return NULL;
		
	/* Create binary object */
	sbin = sieve_binary_create(svinst, script);
	sbin->path = p_strdup(sbin->pool, path);
	sbin->file = file;
	
	if ( !_sieve_binary_open(sbin) ) {
		sieve_binary_unref(&sbin);
		if ( error_r != NULL )
			*error_r = SIEVE_ERROR_NOT_VALID;
		return NULL;
	}
	
	sieve_binary_activate(sbin);
	
	/* Signal open event to extensions */
	regs = array_get(&sbin->extensions, &ext_count);	
	for ( i = 0; i < ext_count; i++ ) {
		const struct sieve_binary_extension *binext = regs[i]->binext;
		
		if ( binext != NULL && binext->binary_open != NULL && 
			!binext->binary_open(regs[i]->extension, sbin, regs[i]->context) ) {
			/* Extension thinks its corrupt */

			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NOT_VALID;

			sieve_binary_unref(&sbin);
			return NULL;
		}
	}	

	return sbin;
}
