/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
//
//  dump.m
//  gctests
//
//  Created by Blaine Garst on 10/22/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import </usr/local/include/auto_zone.h>
#import <objc/objc.h>
#import <objc/objc-auto.h>
#import </usr/local/include/objc/objc-auto-dump.h>
#include <objc/runtime.h>

#if 0
extern void auto_zone_dump(auto_zone_t *zone,
            void (^stack_dump)(const void *base, unsigned long byte_size),
            void (^register_dump)(const void *base, unsigned long byte_size),
            void (^thread_local_node_dump)(const void *address, unsigned long size, unsigned int layout, unsigned long refcount),
            void (^root_dump)(const void **address),
            void (^global_node_dump)(const void *address, unsigned long size, unsigned int layout, unsigned long refcount),
            void (^weak_dump)(const void **address, const void *item)
);
#endif

__weak id Y = nil;

/*
 * Simple zone printer
 */
 
void printLiveDump(auto_zone_t *zone) {
    auto_zone_dump(zone,
        ^(const void *base, unsigned long byte_size)
            { printf("stack base %p, length %lu\n", base, byte_size);},
        ^(const void *base, unsigned long byte_size)
            { printf("register base %p, length %lu\n", base, byte_size);},
        ^(const void *address, unsigned long size, unsigned int layout, unsigned long refcount)
            { printf("thread local %p, size %ld, layout %x, refcount %ld\n", address, size, layout, refcount); },
        ^(const void **address)
            { printf("global root at %p, value there %p\n", address, *address); },
        ^(const void *address, unsigned long size, unsigned int layout, unsigned long refcount)
            { printf("non-local %p, size %ld, layout %x, refcount %ld\n", address, size, layout, refcount); },
        ^(const void **address, const void *item)
            { printf("weak ref located at %p referencing %p\n", address, item); }
    );
}

#if 0

/*
 *  Raw file format definitions
 */
 
// must be unique in first letter...
// RAW FORMAT
#define HEADER      "dumpster"
#define THREAD      't'
#define LOCAL       'l'
#define NODE        'n'
#define REGISTER    'r'
#define ROOT        'g'
#define WEAK        'w'
#define CLASS       'c'
#define END         'e'

#define SixtyFour 1
#define Little    2

/*

Raw format, not that anyone should really care.

<rawfile := <header> <arch> <middle>* <end>
<header> :=  'd' 'u' 'm' 'p' 's' 't' 'e' 'r'                    ; the HEADER string
<arch>   :=  SixtyFour? + Little?                               ; architecture
<middle> := <thread> | <root> | <node> | <weak> | <class>
<thread> := <register> <stack> <local>*                         ; the triple
<register>      := 'r' longLength [bytes]                       ; the register bank
<stack>         := 't' longLength [bytes]                       ; the stack
<local>         := 'l' [long]                                   ; a thread local node
<root>          := 'g' longAddress longValue
<node>          := 'n' longAddress longSize intLayout longRefcount longIsa?
<weak>          := 'w' longAddress longValue
<class>         := 'c' longAddress <name> <strongLayout> <weakLayout>
<name>          := intLength [bytes]                            ; no null byte
<strongLayout>  := intLength [bytes]                            ; including 0 byte at end
<weakLayout>    := intLength [bytes]                            ; including 0 byte at end
<end>           := 'e'

 */
#endif

// COOKED FORMAT
#define HEADER2 "dumpfile"

/*
 * Utilities
 */

static char myType() {
    char type = 0;
    if (sizeof(void *) == 8) type |= SixtyFour;
#if __LITTLE_ENDIAN__
    type |= Little;
#endif
    return type;
}

#if 0

/*
 *  Sigh, a mutable set.
 */
 
typedef struct {
    long *items;
    long count;
    long capacity;
} pointer_set_t;

static pointer_set_t *new_pointer_set() {
    pointer_set_t *result = malloc(sizeof(pointer_set_t));
    result->items = calloc(64, sizeof(long));
    result->count = 0;
    result->capacity = 63;  // last valid ptr, also mask
    return result;
}

static void pointer_set_grow(pointer_set_t *set);

static void pointer_set_add(pointer_set_t *set, long ptr) {
    long hash = ptr & set->capacity;
    while (true) {
        if (!set->items[hash]) {
            set->items[hash] = ptr;
            ++set->count;
            if (set->count*3 > set->capacity*2)
                pointer_set_grow(set);
            return;
        }
        if (set->items[hash] == ptr) return;
        hash = (hash + 1) & set->capacity;
    }
}

static void pointer_set_grow(pointer_set_t *set) {
    long oldCapacity = set->capacity;
    long *oldItems = set->items;
    set->count = 0;
    set->capacity = 2*(oldCapacity+1)-1;
    set->items = calloc(2*(oldCapacity+1), sizeof(long));
    for (long i = 0; i < oldCapacity; ++i)
        if (oldItems[i]) pointer_set_add(set, oldItems[i]);
    free(oldItems);
}

static void pointer_set_iterate(pointer_set_t *set, void (^block)(long item)) {
    for (long i = 0; i < set->capacity; ++i)
        if (set->items[i]) block(set->items[i]);
}

static pointer_set_dispose(pointer_set_t *set) {
    free(set->items);
    free(set);
}

/*
   Quickly dump heap to a named file in a pretty raw format.
 */
void auto_zone_create_dump_file(auto_zone_t *zone, const char *filename) {
    // just write interesting info to disk
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("couldn't open test file: %s", filename);
        return;
    }
    
    fwrite(HEADER, strlen(HEADER), 1, fp);
    char type = myType();
    fwrite(&type, 1, 1, fp);
    
    // for each thread...
    
    // do registers first
    void (^dump_registers)(const void *, unsigned long) = ^(const void *base, unsigned long byte_size) {
        char type = REGISTER;
        fwrite(&type, 1, 1, fp);
        //fwrite(REGISTER, strlen(REGISTER), 1, fp);
        fwrite(&byte_size, sizeof(byte_size), 1, fp);
        fwrite(base, byte_size, 1, fp);
    };
    
    // then stacks
    void (^dump_stack)(const void *, unsigned long) = ^(const void *base, unsigned long byte_size) {
        char type = THREAD;
        fwrite(&type, 1, 1, fp);
        //fwrite(THREAD, strlen(THREAD), 1, fp);
        fwrite(&byte_size, sizeof(byte_size), 1, fp);
        fwrite(base, byte_size, 1, fp);
    };
    
    // then locals
    void (^dump_local)(const void *, unsigned long, unsigned int, unsigned long) =
        ^(const void *address, unsigned long size, unsigned int layout, unsigned long refcount) {
            // just write the value - rely on it showing up again as a node later
            char type = LOCAL;
            fwrite(&type, 1, 1, fp);
            fwrite(&address, sizeof(address), 1, fp);
    };
    
    
    
    // roots
    void (^dump_root)(const void **) = ^(const void **address) {
        char type = ROOT;
        fwrite(&type, 1, 1, fp);
        // write the address so that we can catch misregistered globals
        fwrite(&address, sizeof(address), 1, fp);
        // write content, even (?) if zero
        fwrite(address, sizeof(*address), 1, fp);
    };
    
    // the nodes
    pointer_set_t *classes = new_pointer_set();
    void (^dump_node)(const void *, unsigned long, unsigned int, unsigned long) =
        ^(const void *address, unsigned long size, unsigned int layout, unsigned long refcount) {
            char type = NODE;
            fwrite(&type, 1, 1, fp);
            fwrite(&address, sizeof(address), 1, fp);
            fwrite(&size, sizeof(size), 1, fp);
            fwrite(&layout, sizeof(layout), 1, fp);
            fwrite(&refcount, sizeof(refcount), 1, fp);
            if ((layout & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
                // now the nodes unfiltered content
                fwrite(address, size, 1, fp);
            }
            if ((layout & AUTO_OBJECT) == AUTO_OBJECT) {
#if 0
                id theClass = *(id *)address;
                if (theClass) [classes addObject:theClass];
#else
                long theClass = *(long *)address;
                if (theClass) pointer_set_add(classes, theClass);
#endif
            }
    };
    
    // weak
    void (^dump_weak)(const void **, const void *) = ^(const void **address, const void *item) {
        char type = WEAK;
        fwrite(&type, 1, 1, fp);
        fwrite(&address, sizeof(address), 1, fp);
        fwrite(&item, sizeof(item), 1, fp);
    };

    auto_zone_dump(zone, dump_stack, dump_registers, dump_local, dump_root, dump_node, dump_weak);
    
    //for (Class class in classes) {
    pointer_set_iterate(classes, ^(long class) {
        char type = CLASS;
        fwrite(&type, 1, 1, fp);
        fwrite(&class, sizeof(class), 1, fp);   // write address so that we can map it from node isa's
        // classname (for grins)
        const char *className = class_getName(class);
        unsigned int length = strlen(className);
        fwrite(&length, sizeof(length), 1, fp);      // n
        fwrite(className, length, 1, fp);          // n bytes
        // strong layout
        const char *layout = class_getIvarLayout(class);
        length = layout ? strlen(layout)+1 : 0; // format is <skipnibble><count nibble> ending with <0><0>
        fwrite(&length, sizeof(length), 1, fp);      // n
        fwrite(layout, length, 1, fp);            // n bytes
        // weak layout
        layout = class_getWeakIvarLayout(class);
        length = layout ? strlen(layout)+1 : 0; // format is <skipnibble><count nibble> ending with <0><0>
        fwrite(&length, sizeof(length), 1, fp);      // n
        fwrite(layout, length, 1, fp);             // n bytes
    });

    {
        // end
        char type = END;
        fwrite(&type, 1, 1, fp);
        fclose(fp);
    }
}

#endif

/*
 * Primitives for reading raw file
 */

unsigned long readLong(FILE *fp) {
    unsigned long result;
    size_t len = fread(&result, sizeof(result), 1, fp);
    if (len != 1) {
        printf("error reading long\n");
        exit(1);
    }
    return result;
}
void *readPointer(FILE *fp) {
    void *result;
    size_t len = fread(&result, sizeof(result), 1, fp);
    if (len != 1) {
        printf("error reading pointer\n");
        exit(1);
    }
    return result;
}
int readInt(FILE *fp) {
    int result;
    size_t len = fread(&result, sizeof(result), 1, fp);
    if (len != 1) {
        printf("error reading int\n");
        exit(1);
    }
    return result;
}
__strong char *readString(FILE *fp) {
    int length;
    size_t len = fread(&length, sizeof(length), 1, fp);
    if (len != 1) {
        printf("error reading length\n");
        exit(1);
    }
    if (length == 0) return NULL;
    char buffer[length+1];
    len = fread(buffer, length, 1, fp);
    if (len != 1) {
        printf("error reading string content\n");
        exit(1);
    }
    char *result = NSAllocateCollectable(length+1, 0);
    memmove(result, buffer, length);
    result[length] = 0;
    return result;
}
__strong char *readStringSize(FILE *fp, uint32_t length) {
    if (length == 0) return NULL;
    char buffer[length+1];
    int len = fread(buffer, length, 1, fp);
    if (len != 1) {
        printf("error reading string content\n");
        exit(1);
    }
    char *result = NSAllocateCollectable(length+1, 0);
    memmove(result, buffer, length);
    result[length] = 0;
    return result;
}

FILE *openDump(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Whoops, can't open %s for reading\n", filename);
        return NULL;
    }
    char header[strlen(HEADER)+1];
    fread(header, strlen(HEADER), 1, fp);
    if (memcmp(header, HEADER, strlen(HEADER))) {
        header[strlen(HEADER)] = 0;
        printf("Whoops, header not recognized: %s\n", header);
        return NULL;
    }
    fgetc(fp); // skip type for now
    return fp;
}

FILE *openCooked(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Whoops, can't open %s for reading\n", filename);
        return NULL;
    }
    char header[strlen(HEADER2)+1];
    fread(header, strlen(HEADER2), 1, fp);
    if (memcmp(header, HEADER2, strlen(HEADER2))) {
        header[strlen(HEADER2)] = 0;
        printf("Whoops, header not recognized: %s\n", header);
        return NULL;
    }
    return fp;
}

/*
    Read raw dump and print out its contents without much processing.
    Just a utility printer - or base for better efforts
 */
void printRawDumpFile(const char *filename) {
    FILE *fp = openDump(filename);
    if (!fp) {
        printf("Whoops, can't open %s for reading\n", filename);
        return;
    }
    while (true) {
        int ch = fgetc(fp);
        switch (ch) {
            case EOF: {
                printf("unexpected end of input\n");
                return;
            }
            case THREAD: {
                unsigned long byte_size = readLong(fp);
                printf("thread of size %ld bytes\n", byte_size);
                fseek(fp, byte_size, SEEK_CUR);
                break;
            }
            case REGISTER: {
                unsigned long byte_size = readLong(fp);
                printf("registers of size %ld bytes\n", byte_size);
                fseek(fp, byte_size, SEEK_CUR);
                break;
            }
            case LOCAL: {
                void *address = readPointer(fp);
                printf("local node at %p\n", address);
                break;
            }
            case ROOT: {
                void *address = readPointer(fp);
                void *value = readPointer(fp);
                printf("root at %p value %p\n", address, value);
                break;
            }
            case NODE: {
                void *address = readPointer(fp);
                unsigned long size = readLong(fp);
                int layout = readInt(fp);
                unsigned long refcount = readLong(fp);
                printf("node at %p, size %ld, layout %x, refcount %ld\n", address, size, layout, refcount);
                if ((layout & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
                    fseek(fp, size, SEEK_CUR);
                }
                break;
            }
            case WEAK: {
                void *address = readPointer(fp);
                void *value = readPointer(fp);
                printf("weak at %p value %p\n", address, value);
                break;
            }
            case CLASS: {
                void *address = readPointer(fp);
                char *name = readString(fp);
                printf("class %s at %p", name, address);
                char *layout = readString(fp);
                if (layout) printf(" hasStrong");
                layout = readString(fp);
                if (layout) printf(" hasWeak");
                printf("\n");
                break;
            }
            case END: {
                fclose(fp);
                return;
            }
            default:
                printf("unrecognized key: %x, '%c'\n", ch, ch);
                exit(1);
        }
    }
}

#pragma mark dump crunching

/*
    Define structures for dealing with processed data.
 */
 
typedef struct {
    uint32_t class_index;
    uint32_t nameOffset, nameLen;
    uint32_t strongOffset, strongLen;
    uint32_t weakOffset, weakLen;
} class_descriptor_t;

typedef struct {
    void *original;
    uint32_t size, layout, refcount;
    uint32_t nitems;
    struct {
        uint32_t offset;    // in words
        uint32_t index;
    } items[];
} node_descriptor_t;

typedef struct {
    uint32_t nitems;
    struct {
        uint32_t offset;    // in words
        uint32_t index;
    } items[];
} stack_descriptor_t;

typedef struct thread_descriptor {
    stack_descriptor_t *__strong stack_descriptor;
    uint32_t nLocals;
    uint32_t *locals;
    uint32_t nRegisters;
    uint32_t *registers;
} thread_descriptor_t;


node_descriptor_t *getNodeDescriptor(char *buffer, uint32_t size, char *nibbles, NSMapTable *nodeIndex) {
    char *nibbler = nibbles;
    id *fields = (id *)buffer;
    uint32_t limit = size/sizeof(id);
    uint32_t counter = 0;
    uint32_t nitems = 0;
    // allocate worst case.
    node_descriptor_t *descriptor = NSAllocateCollectable(sizeof(node_descriptor_t)+limit*2*sizeof(uint32_t), 0);
    while (counter < limit) {
        if (nibbler && *nibbler) {
            int skip = ((*nibbler) >> 4 & 0xf);
            counter += skip;
            int use = (*nibbler) & 0xf;
            while (use) {
                long index = (long)[nodeIndex objectForKey:(id)fields[counter]];
                if (index) {
                    descriptor->items[nitems].offset = counter;
                    descriptor->items[nitems].index = index - 1;
                    ++nitems;
                }
                --use;
                ++counter;
            }    
            nibbler++;
        }
        else {
            long index = (long)[nodeIndex objectForKey:(id)fields[counter]];
            if (index) {
                descriptor->items[nitems].offset = counter;
                descriptor->items[nitems].index = index - 1;
                ++nitems;
            }
            ++counter;
        }
    }
    descriptor->nitems = nitems;
    return descriptor;
}


/*
 * raw file reader
 * cooked file writer
 */
 
@interface Dumpster : NSObject {
    NSPointerArray *classArray;
    NSPointerArray *nodeArray;
    NSPointerArray *rootArray;
    NSPointerArray *stackArray;
}
- initWithRawFilename:(const char *)filename verbose:(bool) verbose;
- (void)writeCookedToFilename:(const char *)filename;
@end


@implementation Dumpster

/*
    Read raw format from disk & canonicalize the class & node references.
    We assume less than 4G nodes so we use uint_32 indexes to represent them.
    Ergo #classes too.
    Thread stacks are represented as { int32_t skip, int32_t index }.
    This does not preserve stale references to free nodes.
*/
- initWithRawFilename:(const char *)filename verbose:(bool) verbose {
    FILE *fp = openDump(filename);
    if (!fp) {
        printf("Whoops, can't open %s for reading\n", filename);
        return nil;
    }
    
    // set up class list & mapping tables
    // first, a  structure thing...
    NSPointerFunctions *structDescriptorFunctions =
        [NSPointerFunctions pointerFunctionsWithOptions:
             NSPointerFunctionsStrongMemory
            |NSPointerFunctionsStructPersonality];
    // the array...
    classArray = [NSPointerArray pointerArrayWithPointerFunctions:structDescriptorFunctions];
    // now the pointer keys...
    NSPointerFunctions *pointerFunctions = 
        [NSPointerFunctions pointerFunctionsWithOptions:
             NSPointerFunctionsOpaqueMemory
            |NSPointerFunctionsOpaquePersonality];
    // now the maptable...
    NSMapTable *classMap = [[NSMapTable alloc] initWithKeyPointerFunctions:pointerFunctions valuePointerFunctions:structDescriptorFunctions capacity:32];
    
    // set up mapping of nodes to their next life index
    NSMapTable *nodeIndex = [[NSMapTable alloc] initWithKeyPointerFunctions:pointerFunctions valuePointerFunctions:pointerFunctions capacity:32];
    
    // set up a hashTable (set) of local nodes
    NSHashTable *locals = [[NSHashTable alloc] initWithPointerFunctions:pointerFunctions capacity:32];
    
    nodeArray = [NSPointerArray pointerArrayWithPointerFunctions:structDescriptorFunctions];
    rootArray = [NSPointerArray pointerArrayWithPointerFunctions:pointerFunctions];
    stackArray = [NSPointerArray pointerArrayWithPointerFunctions:structDescriptorFunctions];

    // pass one, establish nodes in heap, set up class table
    // pass two, process node references + everything else
    int pass = 1;
    int nthreads = 0;
    thread_descriptor_t *thread_descriptor = NULL;
    while (true) {
        int ch = fgetc(fp);
        switch (ch) {
            case EOF: {
                printf("unexpected end of input\n");
                exit(1);
            }
            case REGISTER: {     // REGISTER
                unsigned long byte_size = readLong(fp);
                if (pass == 1) {
                    fseek(fp, byte_size, SEEK_CUR);
                    thread_descriptor = NSAllocateCollectable(sizeof(thread_descriptor_t), NSScannedOption);
                    [stackArray addPointer:thread_descriptor];
                    break;
                }
                thread_descriptor = (thread_descriptor_t *)[stackArray pointerAtIndex:nthreads++];
                uint32_t nitems = byte_size/sizeof(void *);
                thread_descriptor->nRegisters = nitems;
                thread_descriptor->registers = (uint32_t *)NSAllocateCollectable(nitems*sizeof(uint32_t), 0);
                for (uint32_t i = 0; i < nitems; ++i) {
                    void *pointer = (void *)readLong(fp);
                    long index = (long)[nodeIndex objectForKey:pointer];
                    thread_descriptor->registers[i] = index;   // +1 for sanity
                }
                break;
            }
            case THREAD: {
                // XXX not complete thread description - need register state also
                unsigned long byte_size = readLong(fp);
                if (pass == 1) {
                    fseek(fp, byte_size, SEEK_CUR);
                    //thread_descriptor = NSAllocateCollectable(sizeof(thread_descriptor_t), NSScannedOption);
                    //[stackArray addPointer:thread_descriptor];
                    break;
                }
                printf("initWithRaw: thread of size %ld bytes\n", byte_size);
                long *stack = (long *)malloc(byte_size);
                fread(stack, byte_size, 1, fp);
                // pass one, fill each "long" with index+1 of node it references, if any
                uint32_t limit = byte_size/sizeof(long);
                uint32_t nitems = 0;
                for (int i = 0; i < limit; ++i) {
                    bool isLocal = [locals member:(id)stack[i]];

                    stack[i] = (long)[nodeIndex objectForKey:(id)stack[i]];
                    if (isLocal) stack[i] = -stack[i];  // negate index for locals
                    if (stack[i]) ++nitems;
                }
                printf("thread size %ld (%ld items) there were %d references found\n", byte_size, byte_size/sizeof(long), nitems);
                // now allocate an appropriately sized stack descriptor
                unsigned long size = nitems*2*sizeof(uint32_t) + sizeof(stack_descriptor_t);
                stack_descriptor_t *descriptor = (stack_descriptor_t *)NSAllocateCollectable(size, 0);
                
                // and fill it up
                descriptor->nitems = nitems;
                int stackWalker = 0;
                int descriptorWalker = 0;
                for (; stackWalker < limit; ++stackWalker) {
                    if (stack[stackWalker]) {
                        descriptor->items[descriptorWalker].offset = stackWalker;
                        descriptor->items[descriptorWalker].index = stack[stackWalker] - 1;
                        ++descriptorWalker;
                    }
                }
                if (descriptorWalker != nitems) {
                    printf("whoops, we set up %d descriptors but should have had %d\n", descriptorWalker, nitems);
                    return nil;
                }
                free(stack);
                if (verbose) printf("stack descriptor %p (%ld) has %d items\n", descriptor, [stackArray count], nitems);
                
                //thread_descriptor = (thread_descriptor_t *)[stackArray pointerAtIndex:nthreads++];
                thread_descriptor->stack_descriptor = descriptor;
                break;
            }
            case LOCAL: {
                void *address = readPointer(fp);
                printf("initWithRaw: local node at %p\n", address);
                if (pass == 1) {
                    [locals addObject:(id)address];
                    thread_descriptor->nLocals++;
                    break;
                }
                if (!thread_descriptor->locals) {
                    thread_descriptor->locals = NSAllocateCollectable(thread_descriptor->nLocals*sizeof(uint32_t), 0);
                    thread_descriptor->nLocals = 0;
                }
                long index = (long)[nodeIndex objectForKey:(id)address];
                thread_descriptor->locals[thread_descriptor->nLocals++] = index;
                break;
            }
            case ROOT: {
                void *address = readPointer(fp);
                void *value = readPointer(fp);
                if (pass == 1) break;   // wait till pass 2
                long index = (long)[nodeIndex objectForKey:(id)value];
                if (verbose > 1) printf("root at %p value %p, index %ld\n", address, value, index);
                if (index == 0) {
                    printf("Hmm, root points to non-node ??\n");
                }
                else {
                    [rootArray addPointer:(void *)(index-1)];
                }
                break;
            }
            case NODE: {
                void *address = readPointer(fp);
                
                unsigned long size = readLong(fp);
                int layout = readInt(fp);
                unsigned long refcount = readLong(fp);
                
                if (pass == 1) {
                    // use +1 so that node 0 shows up; this is internal only, won't go to file that way
                    [nodeIndex setObject:(id)(1 + [nodeIndex count]) forKey:(id)address];
                    
                    //printf("node at %p, size %ld, layout %x, refcount %ld\n", address, size, layout, refcount);
                    if ((layout & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
                        fseek(fp, size, SEEK_CUR);
                    }
                }
                else {
                    if (verbose > 1) printf("node at %p, size %ld, layout %x, refcount %ld\n", address, size, layout, refcount);
                    node_descriptor_t *nodeDescriptor = NULL;
                    if ((layout & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
                        char buffer[size];
                        fread(buffer, size, 1, fp);
                        char *nibbles = NULL;
                        if ((layout & AUTO_OBJECT) == AUTO_OBJECT) {
                            id class;
                            memmove(&class, buffer, sizeof(class));
                            class_descriptor_t *descriptor = (class_descriptor_t *)[classMap objectForKey:class];
                            layout |= (descriptor->class_index << 8);
                            if (!descriptor) {
                                printf("Hmm, object doesn't have a known class\n");
                            }
                            else if (descriptor->strongLen) {
                                nibbles = ((char *)descriptor) + descriptor->strongOffset; // XXX must write null byte
                            }
                        }
                        nodeDescriptor = getNodeDescriptor(buffer, size, nibbles, nodeIndex);
                    }
                    else {
                        nodeDescriptor = NSAllocateCollectable(sizeof(node_descriptor_t), 0);
                        nodeDescriptor->nitems = 0;
                    }
                    nodeDescriptor->original = address;
                    nodeDescriptor->size = size;
                    if ([locals member:(id)address]) {
printf("**** found %p as a local\n", address);
                        layout |= 128;              // mark it as a local
                    }
                    nodeDescriptor->layout = layout;
                    nodeDescriptor->refcount = refcount;
                    //printf("adding node descriptor %p\n", nodeDescriptor);
                    [nodeArray addPointer:nodeDescriptor];
                }
                break;
            }
            case WEAK: {
                void *address = readPointer(fp);
                void *value = readPointer(fp);
                if (verbose) printf("weak at %p value %p\n", address, value);
                break;
            }
            case CLASS: {
                void *address = readPointer(fp);
                char *name = readString(fp);
                char *slayout = readString(fp);
                char *wlayout = readString(fp);
                
                if (pass > 1) break;
                
                if (verbose) printf("class %s at %p", name, address);
                uint32_t dlen = sizeof(class_descriptor_t) + strlen(name);
                if (slayout) if (verbose) printf(" hasStrong");
                if (slayout) dlen += strlen(slayout);
                if (wlayout) if (verbose) printf(" hasWeak");
                if (wlayout) dlen += strlen(wlayout);
                if (verbose) printf("\n");
                class_descriptor_t *descriptor = NSAllocateCollectable(sizeof(class_descriptor_t)+dlen, 0);
                descriptor->class_index = [classMap count]; // used in node_descriptor
                descriptor->nameOffset = sizeof(class_descriptor_t);
                descriptor->nameLen = strlen(name)+1;
                descriptor->strongOffset = descriptor->nameOffset + descriptor->nameLen;
                descriptor->strongLen = slayout ? (strlen(slayout)+1) : 0;
                descriptor->weakOffset = descriptor->strongOffset + descriptor->strongLen;
                descriptor->weakLen = wlayout ? (strlen(wlayout)+1) : 0;
                
                memmove(((char *)descriptor) + descriptor->nameOffset, name, descriptor->nameLen);
                if (slayout) memmove(((char *)descriptor) + descriptor->strongOffset, slayout, descriptor->strongLen);
                if (wlayout) memmove(((char *)descriptor) + descriptor->weakOffset, wlayout, descriptor->weakLen);
                [classArray addPointer:(id)descriptor];
                [classMap setObject:(id)descriptor forKey:address];
                break;
            }
            case END: {
                if (pass == 1) {
                    fclose(fp);
                    fp = openDump(filename);
                    if (!fp) {
                        printf("Whoops, can't reopen %s for reading\n", filename);
                        return nil;
                    }
                    ++pass;
                    if (verbose) printf("!!!! done with first pass!!!!\n");
                }
                else {
                    if (verbose) printf("!!!! done with second pass!!!!\n");
                    fclose(fp);
                    return self;
                }
                break;
            }
            default: {
                printf("unrecognized key: %x, '%c'\n", ch, ch);
                exit(1);
            }
        }
    }
}

- (void)writeCookedToFilename:(const char *)filename {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("could not open %s for writing\n", filename);
        exit(1);
        return;
    }
    // write header
    fwrite(HEADER2, strlen(HEADER2), 1, fp);
    // write ObjC class info
    // first.. count!
    uint32_t count = [classArray count];
    fwrite(&count, sizeof(count), 1, fp);
    // and now 'count' of them
    for (unsigned int index = 0; index < [classArray count]; ++index) {
        class_descriptor_t *descriptor = (class_descriptor_t *)[classArray pointerAtIndex:index];
        uint32_t len = sizeof(class_descriptor_t) + descriptor->nameLen + descriptor->strongLen + descriptor->weakLen;
        fwrite(descriptor, len, 1, fp);
    }
    
    // write node info
    // first the count
    count = [nodeArray count];
    fwrite(&count, sizeof(count), 1, fp);
    // and now the nodes
    for (unsigned int index = 0; index < [nodeArray count]; ++index) {
        node_descriptor_t *descriptor = (node_descriptor_t *)[nodeArray pointerAtIndex:index];
        //uint32_t len = sizeof(node_descriptor_t) + descriptor->nitems*2*sizeof(uint32_t);
        uint32_t len = sizeof(node_descriptor_t);
        //printf("writing cooked [%d] orig %p layout %x size %d\n", index, descriptor->original, descriptor->layout, descriptor->size);
        fwrite(descriptor, len, 1, fp);
    }

    // write node links info
    for (unsigned int index = 0; index < [nodeArray count]; ++index) {
        node_descriptor_t *descriptor = (node_descriptor_t *)[nodeArray pointerAtIndex:index];
        uint32_t len = descriptor->nitems*2*sizeof(uint32_t);
        fwrite(&descriptor->items[0], len, 1, fp);
    }

    // write roots info
    // first the count
    count = [rootArray count];
    fwrite(&count, sizeof(count), 1, fp);
    // and now the nodes
    for (unsigned int counter = 0; counter < [rootArray count]; ++counter) {
        long index = (long)[rootArray pointerAtIndex:counter];
        uint32_t intIndex = index;
        fwrite(&intIndex, sizeof(intIndex), 1, fp);
    }

    // write thread info
    count = [stackArray count];
    fwrite(&count, sizeof(count), 1, fp);
    // and now the stacks
    for (unsigned int index = 0; index < [stackArray count]; ++index) {
        thread_descriptor_t *thread_descriptor = (thread_descriptor_t *)[stackArray pointerAtIndex:index];
        stack_descriptor_t *descriptor = thread_descriptor->stack_descriptor;
        uint32_t len = sizeof(stack_descriptor_t) + descriptor->nitems*2*sizeof(uint32_t);
        //printf("descriptor %p says %d items for thread %d\n", descriptor, descriptor->nitems, index);
        fwrite(descriptor, len, 1, fp);
        // now the locals
        len = thread_descriptor->nLocals;
        fwrite(&len, sizeof(uint32_t), 1, fp);
        fwrite(thread_descriptor->locals, len*sizeof(uint32_t), 1, fp);
        // now the registers
        len = thread_descriptor->nRegisters;
        fwrite(&len, sizeof(uint32_t), 1, fp);
        fwrite(thread_descriptor->registers, len*sizeof(uint32_t), 1, fp);
    }

    fclose(fp);
}
@end


/*
 * cooked file reader
 */
 
struct class_map_entry {
    char *className;
    class_descriptor_t diskClass;
};

@interface Heap : NSObject {
    int nClasses;
    class_descriptor_t *__strong *__strong classes;
    int nNodes;
    node_descriptor_t *__strong *__strong node_descriptors;
    int nRoots;
    uint32_t *roots;
    int nThreads;
    thread_descriptor_t *__strong thread_descriptors;
    // weak
    // associations
    
    auto_zone_t *collector;
}
@end

@implementation Heap
- initWithCookedFilename:(const char *)cookedFile {
    FILE *fp = openCooked(cookedFile);
    if (!fp) {
        printf("Couldn't open cooked file: %s\n", cookedFile);
        exit(1);
    }
    
    /*
     * Classes first
     */
    nClasses = readInt(fp);
    // allocate space for the pointers
    printf("Cooked: %d classes to read\n", nClasses);
    classes = (class_descriptor_t **)NSAllocateCollectable(nClasses*sizeof(struct class_descriptor_t *), NSScannedOption);
    // now read in size & then details for each class
    for (uint32_t i = 0; i < nClasses; ++i) {
        class_descriptor_t fixed;
        fread(&fixed, sizeof(class_descriptor_t), 1, fp);
        uint32_t stringsize = fixed.nameLen + fixed.strongLen + fixed.weakLen;
        uint32_t totalsize = sizeof(class_descriptor_t) + stringsize;
        classes[i] = (class_descriptor_t *)NSAllocateCollectable(totalsize, 0);
        memcpy(classes[i], &fixed, sizeof(class_descriptor_t));
        // now pack the 3 strings in right afterwards
        fread(((char *)classes[i])+fixed.nameOffset, stringsize, 1, fp);
        //printf("read class %s\n", ((char *)classes[i])+fixed.nameOffset);
    }
    
    /*
     * Nodes next
     */
    nNodes = readInt(fp);
    printf("Cooked: %d nodes to read\n", nNodes);
    node_descriptors = (node_descriptor_t **)NSAllocateCollectable(nNodes*sizeof(node_descriptor_t *), NSScannedOption);
    for (uint32_t i = 0; i < nNodes; ++i) {
        node_descriptor_t fixed;
        fread(&fixed, sizeof(node_descriptor_t), 1, fp);
        uint32_t totalsize = sizeof(node_descriptor_t) + fixed.nitems * 2 * sizeof(uint32_t);
        node_descriptors[i] = (node_descriptor_t *)NSAllocateCollectable(totalsize, 0);
        memcpy(node_descriptors[i], &fixed, sizeof(node_descriptor_t));
    }
    for (uint32_t i = 0; i < nNodes; ++i) {
        uint32_t nitems = node_descriptors[i]->nitems;
        uint32_t size = nitems * 2 * sizeof(uint32_t);
        //printf("reading %d links\n", nitems);
        if (nitems) fread(((char *)node_descriptors[i])+sizeof(node_descriptor_t), size, 1, fp);
    }
    
    /*
     * roots
     */
    nRoots = readInt(fp);
    roots = (uint32_t *)NSAllocateCollectable(nRoots*sizeof(uint32_t), 0);
    printf("Cooked: reading %d roots\n", nRoots);
    fread(roots, nRoots, sizeof(uint32_t), fp);
    
    /*
     * threads
     */
    nThreads = readInt(fp);
    thread_descriptors = (thread_descriptor_t *)NSAllocateCollectable(nThreads*sizeof(thread_descriptor_t), NSScannedOption);
    for (uint32_t i = 0; i < nThreads; ++i) {
        /* do stack descriptor part only */
        stack_descriptor_t stack_descriptor, *descriptorp;
        fread(&stack_descriptor, sizeof(stack_descriptor), 1, fp);
        uint32_t variablesize = stack_descriptor.nitems*2*sizeof(uint32_t);
        uint32_t totalsize = sizeof(stack_descriptor_t) + variablesize;
        descriptorp = (stack_descriptor_t *)NSAllocateCollectable(totalsize, 0);
        memcpy(descriptorp, &stack_descriptor, sizeof(stack_descriptor));
        fread(((char *)descriptorp)+sizeof(stack_descriptor_t), variablesize, 1, fp);
        thread_descriptors[i].stack_descriptor = descriptorp;
        /* now the locals */
        thread_descriptors[i].nLocals = readInt(fp);
        uint32_t size = thread_descriptors[i].nLocals*sizeof(uint32_t);
        thread_descriptors[i].locals = NSAllocateCollectable(size, 0);
        fread(thread_descriptors[i].locals, size, 1, fp);
        /* now the registers */
        thread_descriptors[i].nRegisters = readInt(fp);
        size = thread_descriptors[i].nRegisters*sizeof(uint32_t);
        thread_descriptors[i].registers = NSAllocateCollectable(size, 0);
        fread(thread_descriptors[i].registers, size, 1, fp);
    }
}
@end

void printCookedFilename(const char *filename) {
    FILE *fp = openCooked(filename);
    if (!fp) {
        printf("Couldn't open cooked file: %s\n", filename);
        exit(1);
    }
    uint32_t count = readInt(fp);
    printf("%d classes to read...\n", count);
    struct class_map_entry *classMap = NSAllocateCollectable(count*sizeof(struct class_map_entry), NSScannedOption);
    for (uint32_t i = 0; i < count; ++i) {
        // if we memory mapped the file and put the string info in its own section
        // after the constant sized class info we could simply patch the offset
        // with the base pointer, or something, thinking about longs vs uint32_t..
        fread(&classMap[i].diskClass, sizeof(class_descriptor_t), 1, fp);
        classMap[i].className = readStringSize(fp, classMap[i].diskClass.nameLen);
        readStringSize(fp, classMap[i].diskClass.strongLen);
        readStringSize(fp, classMap[i].diskClass.weakLen);
        printf("read class %s\n", classMap[i].className);
    }

    // now do the nodes...
    count = readInt(fp);
    printf("%d nodes to read...\n", count);
    node_descriptor_t *node_descriptors = (node_descriptor_t *)NSAllocateCollectable(count*sizeof(node_descriptor_t), 1);
    for (uint32_t i = 0; i < count; ++i) {
        node_descriptor_t *node_descriptor = &node_descriptors[i];
        fread(node_descriptor, sizeof(node_descriptor_t), 1, fp);
        //printf("node[%d] %p (%p) size %d, layout %x, nitems %d\n", i, node_descriptor, node_descriptor->original, node_descriptor->size, node_descriptor->layout, node_descriptor->nitems);
    }
    for (uint32_t i = 0; i < count; ++i) {
        node_descriptor_t *node_descriptor = &node_descriptors[i];
        printf("node[%d] (%p) size %d, layout %x, nitems %d", i, node_descriptor->original, node_descriptor->size, node_descriptor->layout, node_descriptor->nitems);
        uint32_t classIndex = node_descriptor->layout >> 8;
        if ((node_descriptor->layout & AUTO_OBJECT) == AUTO_OBJECT) {
            printf(", class: %s", classMap[classIndex].className);
        }
        if ((node_descriptor->layout & 128) == 128) {
            printf(", local");
        }
        printf("\n");
        uint32_t nitems = node_descriptors[i].nitems;
        struct {
            uint32_t offset;
            uint32_t index;
        } items[nitems];
        if (nitems) fread(&items[0], sizeof(items), 1, fp);
        for (uint32_t j = 0; j < nitems; ++j) {
            printf(" [%d] -> node[%d]\n", items[j].offset, items[j].index);
        }
    }
    
    // now do the roots...
    count = readInt(fp);
    printf("%d roots to read...\n", count);
    uint32_t roots[count];
    fread(roots, sizeof(roots), 1, fp);
    for (uint32_t i = 0; i < count; ++i) {
        printf("root [%d]\n", roots[i]);
    }


    // now do the stacks...
    count = readInt(fp);
    printf("%d stacks to read...\n", count);    
    for (uint32_t i = 0; i < count; ++i) {
        stack_descriptor_t stack_descriptor;
        fread(&stack_descriptor, sizeof(stack_descriptor), 1, fp);
        printf("stack %d has %d items\n", i, stack_descriptor.nitems);
        struct {
            uint32_t offset;
            uint32_t index;
        } *items = NSAllocateCollectable(stack_descriptor.nitems*2*sizeof(uint32_t), 0);
        fread(&items[0], stack_descriptor.nitems*2*sizeof(uint32_t), 1, fp);
        for (uint32_t j = 0; j < stack_descriptor.nitems; ++j) {
            // need to use absolute value - 1
            int32_t index = items[j].index;
            bool isLocal = index < 0;
            if (isLocal) index = -index;
            printf(" [%d] -> node[%d] %s\n", items[j].offset, index, isLocal ? "isLocal" : "");
        }
        printf("Locals\n");
        uint32_t nitems = readInt(fp);
        uint32_t *memory = malloc(nitems*sizeof(uint32_t));
        fread(memory, nitems, sizeof(uint32_t), fp);
        for (uint32_t j = 0; j < nitems; ++j) {
            // -1 bias?
            printf("local node[%d]\n", memory[j]);
        }
        free(memory);
        printf("Registers\n");
        nitems = readInt(fp);
        memory = malloc(nitems*sizeof(uint32_t));
        fread(memory, nitems, sizeof(uint32_t), fp);
        for (uint32_t j = 0; j < nitems; ++j) {
            // adjust for -1 bias
            if (memory[j]) printf("register node[%d]\n", memory[j]-1);
        }
        free(memory);
    }
}


char RawName[1024];

void takeRawSnapshot() {
    static id X = nil;
    if (!X) X = [NSObject new];
    printf("Should see %p as a global\n", &X);
    //static __weak id Y = nil;
    Y = X;
    printf("Should see %p as a weak -> %p\n", &Y, X);
    id local = [NSObject new];
    printf("Should see %p as a local\n", local);
    //printLiveDump((auto_zone_t *)NSDefaultMallocZone());
    //auto_zone_create_dump_file((auto_zone_t *)NSDefaultMallocZone(), rawName);
    objc_dumpHeap(RawName, sizeof(RawName));
    //printRawDumpFile("/private/tmp/test.dumpster");
}

void cook(const char *rawName, const char *cookedName) {
    Dumpster *dumpster = [[Dumpster alloc] initWithRawFilename:rawName verbose:false];
    [dumpster writeCookedToFilename:cookedName];
}

void enliven(const char *cookedName) {
    
}


int main(int argc, char *argv[]) {
    //printLiveDump();
    char cookedName[1024];
    if (0) {
        takeRawSnapshot();
        printf("raw file written to %s\n", RawName);
        sprintf(cookedName, "%s.cooked", RawName);
    }
    else {
        strcpy(RawName, argv[1]);
        sprintf(cookedName, "%s.cooked", RawName);
    }
    cook(RawName, cookedName);
    //printCookedFilename(cookedName);
    Heap *heap = [[Heap alloc] initWithCookedFilename:cookedName];
    return 0;
}