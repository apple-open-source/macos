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
/*
    AutoInUseEnumerator.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_INUSEENUMERATOR__
#define __AUTO_INUSEENUMERATOR__

#include <malloc/malloc.h>

#include "AutoDefs.h"
#include "AutoAdmin.h"

namespace Auto {

    //----- InUseEnumerator -----//
    
    class InUseEnumerator {
    
      private:
     
        task_t                      _task;                          // task being probed
        void                        *_context;                      // context passed back to callbacks
        unsigned                    _type_mask;                     // selection of regions to be enumerated
        vm_address_t                _zone_address;                  // task address of zone being probed
        auto_memory_reader_t        _reader;                        // reader used to laod task memory
        auto_vm_range_recorder_t    _recorder;                      // range recording function
        kern_return_t               _error;                         // error from call back
        
      public:
     
        //
        // Constructor
        //
        InUseEnumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, auto_memory_reader_t reader, auto_vm_range_recorder_t recorder)
        : _task(task)
        , _context(context)
        , _type_mask(type_mask)
        , _zone_address(zone_address)
        , _reader(reader)
        , _recorder(recorder)
        , _error(KERN_SUCCESS)
        {}
        
        
        //
        // read
        //
        // Read memory from the task into current memory.
        //
        inline void *read(void *task_address, usword_t size) {
            void *local_address;                           // location where memory was read
            
            kern_return_t err = _reader(_task, (vm_address_t)task_address, (vm_size_t)size, &local_address);
            
            if (err) {
                _error = err;
                return NULL;
            }
            
            return local_address;
        }
        
        
        //
        // record
        //
        // Inform requester of a block's existence.
        //
        inline void record(void *address, usword_t size, unsigned type) {
            // if recording this type
            if (_type_mask & type) {
                // range to record
                vm_range_t range;
                range.address = (vm_address_t)address;
                range.size = size;
                // record
                _recorder(_task, _context, type & _type_mask, &range, 1);
            }
        }
        
      
        //
        // scan
        //
        // Scan through a task's auto zone looking for 
        //
        kern_return_t scan();


    };
    
    
};

#endif // __AUTO_INUSEENUMERATOR__

