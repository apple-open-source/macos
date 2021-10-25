/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <stdlib.h>
#include <os/collections.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>


T_DECL(map_basic_64, "Make sure 64 bit map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_64_t basic_64_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeef = false;
	uint64_t value;

	T_LOG("Start");

	// *** BASIC 64 bit key testing ***

	os_map_init(&basic_64_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 0, "Expect map to be empty");

	os_map_insert(&basic_64_map, 0xCAFECAFE, (void *)0xCAFEBABE);
	os_map_insert(&basic_64_map, 0xDEADDEAD, (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_64_map, ^bool (uint64_t key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (key == 0xCAFECAFE) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (key == 0xDEADDEAD) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeef = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeef) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_64_map, 0xFF00F0F0);
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");


	os_map_delete(&basic_64_map, 0xDEADDEAD);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_64_map, 0xCAFECAFE);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 0,
		    "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_64_map);
}

T_DECL(map_basic_32, "Make sure 32 bit map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_32_t basic_32_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeef = false;
	uint64_t value;

	T_LOG("Start");

	os_map_init(&basic_32_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 0, "Expect map to be empty");

	os_map_insert(&basic_32_map, 0xCAFECAFE, (void *)0xCAFEBABE);
	os_map_insert(&basic_32_map, 0xDEADDEAD, (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_32_map, ^bool (uint32_t key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (key == 0xCAFECAFE) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (key == 0xDEADDEAD) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeef = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeef) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_32_map, 0xFF00F0F0);
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");

	os_map_delete(&basic_32_map, 0xDEADDEAD);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_32_map, 0xCAFECAFE);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 0, "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_32_map);
}


T_DECL(map_basic_string, "Make sure string map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_str_t basic_string_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeef = false;
	uint64_t value;

	T_LOG("Start");

	os_map_init(&basic_string_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_string_map), 0,
		    "Expect map to be empty");

	os_map_insert(&basic_string_map, "0xCAFECAFE", (void *)0xCAFEBABE);
	os_map_insert(&basic_string_map, "0xDEADDEAD", (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_string_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_string_map, ^bool (const char *key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (strcmp("0xCAFECAFE", key) == 0) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (strcmp("0xDEADDEAD", key) == 0) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeef = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeef) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_string_map, "0xFF00F0F0");
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");


	os_map_delete(&basic_string_map, "0xDEADDEAD");

	T_ASSERT_EQ(os_map_count(&basic_string_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_string_map, "0xCAFECAFE");

	T_ASSERT_EQ(os_map_count(&basic_string_map), 0,
		    "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_string_map);
}

T_DECL(map_entry_string, "Make sure string entry fetching works",
		T_META("owner", "Core Darwin Daemons & Tools"))
{

	os_map_str_t basic_string_map;

	T_LOG("Start");

	os_map_init(&basic_string_map, NULL);

	os_map_insert(&basic_string_map, "CAFE", (void *)0xCAFEBABE);

	// Extra steps are taken to make sure the lookup strings aren't compiled
	// to the same pointer.
	volatile char lookup_string_1[5];
	sprintf(lookup_string_1, "CAFE");
	volatile char lookup_string_2[5];
	sprintf(lookup_string_2, "CAFE");

	T_ASSERT_EQ(strcmp(&lookup_string_1, "CAFE"), 0,
		    "Expect lookup strings to be CAFE");
	T_ASSERT_EQ(strcmp(&lookup_string_2, "CAFE"), 0,
		    "Expect lookup strings to be CAFE");
	T_ASSERT_NE(&lookup_string_1, &lookup_string_2,
		    "Expect lookup strings to be different");

	const char *entry_string_1 = os_map_entry(&basic_string_map,
						  &lookup_string_1);

	T_ASSERT_NOTNULL(entry_string_1, "Expect entry strings to be nonnull");
	T_ASSERT_EQ(strcmp(entry_string_1, "CAFE"), 0,
		    "Expect entry strings to be CAFE");

	const char *entry_string_2 = os_map_entry(&basic_string_map,
						  &lookup_string_2);

	T_ASSERT_NE(entry_string_2, NULL, "Expect entry strings to be nonnull");
	T_ASSERT_EQ(strcmp(entry_string_2, "CAFE"), 0,
		    "Expect entry strings to be CAFE");

	T_ASSERT_EQ(entry_string_1, entry_string_2,
		    "Expect entry strings to be literally equal");

	os_map_destroy(&basic_string_map);
}

T_DECL(map_basic_128, "Make sure 64 bit map basics work",
        T_META("owner", "Core Darwin Daemons & Tools"))
{
    os_map_128_t basic_128_map;
    __block bool got_cafebabe = false;
    __block bool got_deadbeef = false;
    uint64_t value;

    T_LOG("Start");

    // *** BASIC 64 bit key testing ***

    os_map_init(&basic_128_map, NULL);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 0, "Expect map to be empty");

    os_map_128_key_t key;
    
    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    os_map_insert(&basic_128_map, key, (void *)0xCAFEBABE);
    
    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    os_map_insert(&basic_128_map, key, (void *)0xDEADBEEF);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 2, "Expect map to have 2 entries");

    os_map_foreach(&basic_128_map, ^bool (os_map_128_key_t key, void *value){
        T_LOG("Foreach called for 0x%llx:0x%llx, 0x%llx",
	      (unsigned long long)key.x[0], (unsigned long long)key.x[1],
	      (unsigned long long)value);
        if (key.x[0] == 0xCAFECAFE && key.x[1] == 0xBABEBABE) {
            T_ASSERT_EQ(value, (void *)0xCAFEBABE,
			"Callback expect 0xCAFEBABE");
            got_cafebabe = true;
        } else if (key.x[0] == 0xDEADDEAD && key.x[1] == 0xBEEFBEEF) {
            T_ASSERT_EQ(value, (void *)0xDEADBEEF, "Callback expec 0xDEADBEEF");
            got_deadbeef = true;
        } else {
            T_FAIL("Got unexpected callback 0x%llx:0x%llx, 0x%llx",
		   (unsigned long long)key.x[0], (unsigned long long)key.x[1],
		   (unsigned long long)value);
        }
        return true;
    });

    if (!got_cafebabe || !got_deadbeef) {
        T_FAIL("Failed to get callback");
    }

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 1");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 2");

    key.x[0] = 0xFF00F0F0;
    key.x[1] = 0xFF00F0F0;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");
    
    key.x[0] = 0xFF00F0F0;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 4");
    
    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xFF00F0F0;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 5");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    os_map_delete(&basic_128_map, key);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 1,
		"Expect map to have 1 entries");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    os_map_delete(&basic_128_map, key);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 0, "Expect map to be empty");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

    os_map_destroy(&basic_128_map);
}


T_DECL(set_basic_64_ptr, "Make sure 64 ptr set basics work",
        T_META("owner", "Core Darwin Daemons & Tools"))
{
    os_set_64_ptr_t basic_64_ptr_set;
    __block bool got_cafebabe = false;
    __block bool got_deadbeef = false;
    uint64_t cafebabe_holder = 0xCAFEBABE;
    void *cafebabe_holder_adr = &cafebabe_holder;
    T_LOG("Storing cafebabe in 0x%llx", (unsigned long long)cafebabe_holder_adr);
    uint64_t deadbeef_holder = 0xDEADBEEFDEADFA11;
    void *deadbeef_holder_adr = &deadbeef_holder;
    T_LOG("Storing deadbeaf in 0x%llx", (unsigned long long)deadbeef_holder_adr);
    uint64_t *fetch_pointer = NULL;

    T_LOG("Start");

    // *** BASIC 64 ptr testing ***

    os_set_init(&basic_64_ptr_set, NULL);

    T_ASSERT_EQ(os_set_count(&basic_64_ptr_set), 0, NULL);

    os_set_insert(&basic_64_ptr_set, &cafebabe_holder);
    T_ASSERT_EQ(os_set_count(&basic_64_ptr_set), 1, NULL);

    os_set_insert(&basic_64_ptr_set, &deadbeef_holder);
    T_ASSERT_EQ(os_set_count(&basic_64_ptr_set), 2, NULL);

    os_set_foreach(&basic_64_ptr_set, ^bool (uint64_t *iter_pointer){
        T_LOG("Foreach called for 0x%llx (val:0x%llx)",
              (unsigned long long)iter_pointer, (unsigned long long)*iter_pointer);
        if (*iter_pointer == 0xCAFEBABE) {
            got_cafebabe = true;
            T_ASSERT_EQ(iter_pointer, cafebabe_holder_adr, NULL);
        } else if (*iter_pointer  == 0xDEADBEEFDEADFA11) {
            got_deadbeef = true;
            T_ASSERT_EQ(iter_pointer, deadbeef_holder_adr, NULL);
        } else {
            T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
                   (unsigned long long)iter_pointer,
                   (unsigned long long)*iter_pointer);
        }
        return true;
    });

    if (!got_cafebabe || !got_deadbeef) {
        T_FAIL("Failed to get callback");
    }

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xDEADBEEFDEADFA11);
    T_ASSERT_EQ(fetch_pointer, &deadbeef_holder, "Find 1");

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "Find 2");

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xFF00F0F0);
    T_ASSERT_EQ(fetch_pointer, NULL, "Find 3");


    os_set_delete(&basic_64_ptr_set, 0xDEADBEEFDEADFA11);
    T_ASSERT_EQ(os_set_count(&basic_64_ptr_set), 1, NULL);

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xDEADBEEFDEADFA11);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 1");

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "After-delete find 2");


    T_ASSERT_EQ(os_set_delete(&basic_64_ptr_set, 0xCAFEBABE), &cafebabe_holder, NULL);
    T_ASSERT_EQ(os_set_count(&basic_64_ptr_set), 0, NULL);


    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xDEADBEEFDEADFA11);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 3");

    fetch_pointer = os_set_find(&basic_64_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 4");

    os_set_destroy(&basic_64_ptr_set);
}

T_DECL(set_basic_32_ptr, "Make sure 32 ptr set basics work",
        T_META("owner", "Core Darwin Daemons & Tools"))
{
    os_set_32_ptr_t basic_32_ptr_set;
    __block bool got_cafebabe = false;
    __block bool got_deadbeef = false;
    uint32_t cafebabe_holder = 0xCAFEBABE;
    T_LOG("Storing cafebabe in 0x%llx", (unsigned long long)&cafebabe_holder);
    uint32_t deadbeef_holder = 0xDEADBEEF;
    T_LOG("Storing deadbeaf in 0x%llx", (unsigned long long)&deadbeef_holder);
    uint32_t *fetch_pointer = NULL;

    T_LOG("Start");

    // *** BASIC 32 ptr testing ***

    os_set_init(&basic_32_ptr_set, NULL);

    os_set_insert(&basic_32_ptr_set, &cafebabe_holder);
    os_set_insert(&basic_32_ptr_set, &deadbeef_holder);

    os_set_foreach(&basic_32_ptr_set, ^bool (uint32_t *iter_pointer){
        T_LOG("Foreach called for 0x%llx (val:0x%lx)",
              (unsigned long long)iter_pointer, (unsigned long)*iter_pointer);
        if (*iter_pointer == 0xCAFEBABE) {
            got_cafebabe = true;
        } else if (*iter_pointer  == 0xDEADBEEF) {
            got_deadbeef = true;
        } else {
            T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
                   (unsigned long long)iter_pointer,
                   (unsigned long long)*iter_pointer);
        }
        return true;
    });

    if (!got_cafebabe || !got_deadbeef) {
        T_FAIL("Failed to get callback");
    }

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xDEADBEEF);
    T_ASSERT_EQ(fetch_pointer, &deadbeef_holder, "Find 1");

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "Find 2");

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xFF00F0F0);
    T_ASSERT_EQ(fetch_pointer, NULL, "Find 3");


    os_set_delete(&basic_32_ptr_set, 0xDEADBEEF);

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xDEADBEEF);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 1");

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "After-delete find 2");


    os_set_delete(&basic_32_ptr_set, 0xCAFEBABE);


    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xDEADBEEF);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 3");

    fetch_pointer = os_set_find(&basic_32_ptr_set, 0xCAFEBABE);
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 4");

    os_set_destroy(&basic_32_ptr_set);
}

T_DECL(set_basic_str_ptr, "Make sure str ptr set basics work",
        T_META("owner", "Core Darwin Daemons & Tools"))
{
    os_set_str_ptr_t basic_str_ptr_set;
    __block bool got_cafebabe = false;
    __block bool got_deadbeef = false;
    const char *cafebabe_holder = "0xCAFEBABE";
    T_LOG("Storing cafebabe in 0x%llx", (unsigned long long)&cafebabe_holder);
    const char *deadbeef_holder = "0xDEADBEEF";
    T_LOG("Storing deadbeaf in 0x%llx", (unsigned long long)&deadbeef_holder);
    const char **fetch_pointer = NULL;

    T_LOG("Start");

    // *** BASIC str ptr testing ***

    os_set_init(&basic_str_ptr_set, NULL);

    os_set_insert(&basic_str_ptr_set, &cafebabe_holder);
    os_set_insert(&basic_str_ptr_set, &deadbeef_holder);


    os_set_foreach(&basic_str_ptr_set, ^bool (const char **iter_pointer){
        T_LOG("Foreach called for 0x%llx (val:%s)",
              (unsigned long long)iter_pointer, *iter_pointer);
        if (strcmp(*iter_pointer, "0xCAFEBABE") == 0) {
            got_cafebabe = true;
        } else if (strcmp(*iter_pointer, "0xDEADBEEF") == 0) {
            got_deadbeef = true;
        } else {
            T_FAIL("Got unexpected callback 0x%llx, %s",
                   (unsigned long long)iter_pointer,
                   *iter_pointer);
        }
        return true;
    });

    if (!got_cafebabe || !got_deadbeef) {
        T_FAIL("Failed to get callback");
    }

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xDEADBEEF");
    T_ASSERT_EQ(fetch_pointer, &deadbeef_holder, "Find 1");

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xCAFEBABE");
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "Find 2");

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xFF00F0F0");
    T_ASSERT_EQ(fetch_pointer, NULL, "Find 3");


    os_set_delete(&basic_str_ptr_set, "0xDEADBEEF");

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xDEADBEEF");
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 1");

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xCAFEBABE");
    T_ASSERT_EQ(fetch_pointer, &cafebabe_holder, "After-delete find 2");


    os_set_delete(&basic_str_ptr_set, "0xCAFEBABE");


    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xDEADBEEF");
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 3");

    fetch_pointer = os_set_find(&basic_str_ptr_set, "0xCAFEBABE");
    T_ASSERT_EQ(fetch_pointer, NULL, "After-delete Find 4");

    os_set_destroy(&basic_str_ptr_set);
}
