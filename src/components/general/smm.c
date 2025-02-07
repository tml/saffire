/*
 Copyright (c) 2012-2013, The Saffire Group
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the Saffire Group the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "general/output.h"
#include "general/hashtable.h"
#include "general/smm.h"


// @TODO: Fix this into a better/faster memory manager (slab allocator)

long smm_malloc_calls = 0;
long smm_realloc_calls = 0;
long string_strdup_calls = 0;

void *smm_malloc(size_t size) {
    smm_malloc_calls++;
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fatal_error(1, "Error while allocating memory (%lu bytes)!\n", (unsigned long)size);        /* LCOV_EXCL_LINE */
    }
    return ptr;
}

void *smm_zalloc(size_t size) {
    void *p = smm_malloc(size);
    bzero(p, size);
    return p;
}

void *smm_realloc(void *ptr, size_t size) {
    smm_realloc_calls++;
    void *newptr = realloc(ptr, size);
    if (newptr == NULL) {
        fatal_error(1, "Error while reallocating memory (%lu bytes)!\n", (unsigned long)size);      /* LCOV_EXCL_LINE */
    }
    return newptr;
}

void smm_free(void *ptr) {
    return free(ptr);
}
