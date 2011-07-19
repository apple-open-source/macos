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

// CONFIG GC -C99

#import <Foundation/Foundation.h>
#import <malloc/malloc.h>
#import <objc/objc-auto.h>
#import <pthread.h>

void *allocFreeALot(void *ignored) {
    unsigned int counter = 0;
    objc_registerThreadWithCollector();
    malloc_zone_t *autoZone = (malloc_zone_t *)NSDefaultMallocZone();
    while (++counter < 100000) {
        //printf("doing the malloc...\n");
        malloc_zone_free(autoZone, malloc_zone_malloc(autoZone, 128));
        //if ((counter & 0xffff) == 0) printf("%x\n", counter);
    }
    //printf("done\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    unsigned int nthreads = 5;
    objc_startCollectorThread();
    pthread_t threads[nthreads];
    for (int i = 0; i < nthreads; ++i)
        pthread_create(&threads[i], NULL, allocFreeALot, NULL);
    for (int i = 0; i < nthreads; ++i) {
        //printf("joining...\n");
        pthread_join(threads[i], NULL);
    }
    printf("%s: success\n", argv[0]);
    return 0;
}