/*
 * sym.c
 * lockstat
 *
 * Created by Samuel Gosselin on 10/1/14.
 * Copyright 2014 Apple Inc. All rights reserved.
 *
 */

#include <CoreSymbolication/CoreSymbolication.h>
#include <CoreSymbolication/CoreSymbolicationPrivate.h>

#include <libkern/OSAtomic.h>
#include <sys/sysctl.h>

#define LAST_SEGMENT_TOKEN	"__LAST SEGMENT"
#define SYMBOL_NAME_MAX_LENGTH	(16)
#define MIN_PAGE_SIZE		(4 * 1024)

typedef struct {
	uintptr_t	location;
	size_t		length;
	char const	*name;
} sym_t;

static sym_t			*g_symtable;
static size_t			g_nbsyms;
static size_t			g_maxsyms;
static CSSymbolicatorRef 	g_symbolicator;

static int
add_symbol(uintptr_t location, size_t length, char const* name)
{
	sym_t *sym;

	if (g_symtable == NULL || g_nbsyms >= g_maxsyms)
		return -1;

	sym = &g_symtable[g_nbsyms++];
	sym->location = location;
	sym->length = length;
	sym->name = name;

	return 0;
}

static int
create_fake_symbols(CSRange table_range)
{
	int i, ncpus;
	size_t len = sizeof(ncpus);
	char *name;

	assert(table_range.length > 0);
	assert(table_range.location > NULL);

	/* retrieve the number of cpus */
	if (sysctlbyname("hw.ncpu", &ncpus, &len, NULL, 0) < 0) {
		fprintf(stderr, "could not retrieve the number of cpus in the system\n");
		return -1;
	}

	assert(ncpus > 0);

	/* Check if we will have enough room for the symbols */
	if (ncpus > table_range.length) {
		fprintf(stderr, "symbols table not big enough to store the fake cpu symbols\n");
		return -1;
	}

	/*
	 * Currently, we're only storing fake symbols for the cpus, and the size
	 * is already known.
	 */
	g_maxsyms = ncpus;
	if (!(g_symtable = malloc(sizeof(sym_t) * ncpus))) {
		fprintf(stderr, "could not allocate memory for the symbols table\n");
		return -1;
	}

	/* allocate a new symbol for each cpu in the system */
	for (i = 0; i < ncpus; ++i) {
		if (!(name = malloc(SYMBOL_NAME_MAX_LENGTH))) {
			fprintf(stderr, "could not allocate memory for the cpu[%d] symbol\n", i);
			return -1;
		}

		(void) snprintf(name, SYMBOL_NAME_MAX_LENGTH, "cpu[%d]", i);

		if (add_symbol(table_range.location + i, 0, name) < 0) {
			fprintf(stderr, "could not add the symbol [%s] to the symbol table", name);
			return -1;
		}
	}

	return 0;
}

static int
find_symtab_range(CSRange *res)
{
	__block CSSegmentRef	last_segment;
	__block CSRange		last_section_range;
	CSRange			last_segment_range;

	assert(res);

	/* locate the kernel last segment */
	CSSymbolicatorForeachSegmentAtTime(g_symbolicator, kCSNow, ^(CSSegmentRef it) {
		if (!strcmp(LAST_SEGMENT_TOKEN, CSRegionGetName(it)))
			last_segment = it;
	});

	/* ensure that the segment can store our symbols table */
	last_segment_range = CSRegionGetRange(last_segment);
	if (last_segment_range.length < MIN_PAGE_SIZE)
		return -1;

	/* locate the last section in the last segment */
	CSSegmentForeachSection(last_segment, ^(CSSectionRef it) {
		last_section_range = CSRegionGetRange(it);
	});

	/* no section found, create an empty section */
	if (!last_section_range.location)
		last_section_range = CSRangeMake(last_segment_range.location, 0);

	/* initialize the symbols table range */
	res->location = CSRangeMax(last_section_range);
	res->length = CSRangeMax(last_segment_range) - res->location;

	/* be sure that we're not returning an incorrect range */
	assert(CSRangeContainsRange(last_segment_range, *res));

	return 0;
}

int
symtab_init(void)
{
	CSRange table_range;

	enum CSSymbolicatorPrivateFlags symflags = 0x0;
	symflags |= kCSSymbolicatorDefaultCreateFlags;
	symflags |= kCSSymbolicatorUseSlidKernelAddresses;

	/* retrieve the kernel symbolicator */
	g_symbolicator = CSSymbolicatorCreateWithMachKernelFlagsAndNotification(symflags, NULL);
	if (CSIsNull(g_symbolicator)) {
		fprintf(stderr, "could not retrieve the kernel symbolicator\n");
		return -1;
	}

	/* retrieve the range for the table */
	if (find_symtab_range(&table_range) < 0) {
		fprintf(stderr, "could not find a valid range for the symbols table\n");
		return -1;
	}

	return create_fake_symbols(table_range);
}

char const*
addr_to_sym(uintptr_t addr, uintptr_t *offset, size_t *sizep)
{
	CSSymbolRef symbol;
	CSRange	range;
	int i;

	assert(offset);
	assert(sizep);

	symbol = CSSymbolicatorGetSymbolWithAddressAtTime(g_symbolicator, addr, kCSNow);
	if (!CSIsNull(symbol)) {
		range = CSSymbolGetRange(symbol);
		*offset = addr - range.location;
		*sizep = range.length;
		return CSSymbolGetName(symbol);
	}

	for (i = 0; i < g_nbsyms; ++i) {
		sym_t* sym = &g_symtable[i];
		if ((addr >= sym->location) && (addr <= sym->location + sym->length)) {
			*offset = addr - sym->location;
			*sizep = sym->length;
			return sym->name;
		}
	}

	return NULL;
}

uintptr_t
sym_to_addr(char *name)
{
	CSSymbolRef symbol;
	int i;

	symbol = CSSymbolicatorGetSymbolWithNameAtTime(g_symbolicator, name, kCSNow);
	if (!CSIsNull(symbol))
		return CSSymbolGetRange(symbol).location;

	for (i = 0; i < g_nbsyms; ++i)
		if (!strcmp(name, g_symtable[i].name))
			return g_symtable[i].location;

	return NULL;
}

size_t
sym_size(char *name)
{
	CSSymbolRef symbol;
	int i;

	symbol = CSSymbolicatorGetSymbolWithNameAtTime(g_symbolicator, name, kCSNow);
	if (!CSIsNull(symbol))
		return CSSymbolGetRange(symbol).length;

	for (i = 0; i < g_nbsyms; ++i)
		if (!strcmp(name, g_symtable[i].name))
			return g_symtable[i].length;

	return NULL;
}

