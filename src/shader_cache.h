/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * shader_cache.h
 * MGL
 *
 * What glslang and SPIRV-Cross made, kept on disk under a hash of what went
 * in, so a later run skips them. Each program has a folder under
 * ~/Library/Caches/MooGL, kept under MGL_SHADER_CACHE_MAX_MB (256 by default).
 * MGL_SHADER_CACHE=0 turns it off, MGL_SHADER_CACHE_DIR moves it, and
 * MGL_SHADER_CACHE_VERIFY=1 does the work anyway and checks it matches.
 */

#ifndef shader_cache_h
#define shader_cache_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <CommonCrypto/CommonDigest.h>

typedef struct {
    CC_SHA256_CTX sha;
} MglHash;

// A key starts with what kind of entry it is and which build of MooGL made it
void mglHashBegin(MglHash *h, const char *kind);
void mglHashBytes(MglHash *h, const void *p, size_t n);
void mglHashU32(MglHash *h, uint32_t v);
// a NULL string hashes differently from an empty one
void mglHashStr(MglHash *h, const char *s);
void mglHashEnd(MglHash *h, char hex[65]);
// glslang's limits, without the padding after them
void mglHashResource(MglHash *h, const void *resource);

typedef struct {
    unsigned char *data;
    size_t len, cap, at;
    bool bad;           // ran out of memory, or read past the end
} MglBlob;

void mglBlobPut(MglBlob *b, const void *p, size_t n);
void mglBlobPutU32(MglBlob *b, uint32_t v);
void mglBlobPutStr(MglBlob *b, const char *s);
bool mglBlobGet(MglBlob *b, void *p, size_t n);
uint32_t mglBlobGetU32(MglBlob *b);
// malloc'd, or NULL for a NULL string
char *mglBlobGetStr(MglBlob *b);
void mglBlobFree(MglBlob *b);

bool mglShaderCacheOn(void);
bool mglShaderCacheVerify(void);
// a blob to read from, or false when there is no entry
bool mglShaderCacheGet(const char *kind, const char *hex, MglBlob *out);
void mglShaderCachePut(const char *kind, const char *hex, const MglBlob *b);

// for tests
void mglShaderCacheUseDir(const char *dir);
unsigned mglShaderCacheHits(void);

#endif /* shader_cache_h */
