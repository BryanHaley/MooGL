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
 * shader_cache.c
 * MGL
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pthread.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "shader_cache.h"
#include <stddef.h>
#include <glslang_c_interface.h>
#include "mgl_log.h"

static const char magic[4] = { 'M', 'G', 'L', 'C' };

// the build this is, so a new MooGL never reads what an old one wrote
static bool buildId(unsigned char id[16])
{
    Dl_info info;

    if (!dladdr((const void *)buildId, &info) || info.dli_fbase == NULL)
        return false;

    const struct mach_header_64 *mh = (const struct mach_header_64 *)info.dli_fbase;
    const struct load_command *lc = (const struct load_command *)(mh + 1);

    for (uint32_t i = 0; i < mh->ncmds; i++)
    {
        if (lc->cmd == LC_UUID)
        {
            memcpy(id, ((const struct uuid_command *)lc)->uuid, 16);
            return true;
        }

        lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
    }

    return false;
}

static unsigned char build_id[16];
static char cache_dir[1024];

typedef struct {
    char *path;
    off_t size;
    time_t used;
} Entry;

static int byAge(const void *a, const void *b)
{
    time_t x = ((const Entry *)a)->used, y = ((const Entry *)b)->used;

    return x < y ? -1 : x > y;
}

// Over the cap, the least recently used entries go until it is a quarter under.
static void trim(const char *dir, off_t cap)
{
    char *roots[] = { (char *)dir, NULL };
    FTS *fts = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    Entry *list = NULL;
    size_t n = 0, room = 0;
    off_t total = 0;
    FTSENT *e;

    if (fts == NULL)
        return;

    while ((e = fts_read(fts)) != NULL)
    {
        if (e->fts_info != FTS_F)
            continue;

        if (n == room)
        {
            Entry *grown = (Entry *)realloc(list, (room = room ? room * 2 : 1024) * sizeof(Entry));

            if (grown == NULL)
                break;
            list = grown;
        }

        list[n].path = strdup(e->fts_path);
        list[n].size = e->fts_statp->st_size;
        list[n].used = e->fts_statp->st_mtime;
        total += list[n].size;
        n++;
    }

    fts_close(fts);

    if (total > cap)
    {
        qsort(list, n, sizeof(Entry), byAge);

        for (size_t i = 0; i < n && total > cap - cap / 4; i++)
            if (list[i].path && unlink(list[i].path) == 0)
                total -= list[i].size;
    }

    for (size_t i = 0; i < n; i++)
        free(list[i].path);
    free(list);
}

static bool have_id;
static unsigned hits;

static void configure(void)
{
    const char *off = getenv("MGL_SHADER_CACHE");
    const char *dir = getenv("MGL_SHADER_CACHE_DIR");
    const char *home = getenv("HOME");

    have_id = buildId(build_id);

    if ((off && !strcmp(off, "0")) || !have_id)
        cache_dir[0] = 0;
    else if (dir && dir[0])
        snprintf(cache_dir, sizeof cache_dir, "%s", dir);
    else if (home && home[0])
        snprintf(cache_dir, sizeof cache_dir, "%s/Library/Caches/MooGL/%s", home, getprogname());

    if (cache_dir[0])
    {
        const char *mb = getenv("MGL_SHADER_CACHE_MAX_MB");
        long cap = mb ? atol(mb) : 256;

        trim(cache_dir, (off_t)(cap > 0 ? cap : 256) << 20);
    }
}

static void setUp(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once(&once, configure);
}

bool mglShaderCacheOn(void)
{
    setUp();
    return cache_dir[0] != 0;
}

// For tests: the folder to use, or NULL for the usual one.
void mglShaderCacheUseDir(const char *dir)
{
    setUp();
    configure();

    if (dir && have_id)
        snprintf(cache_dir, sizeof cache_dir, "%s", dir);
}

// For tests: how many entries have been read and used.
unsigned mglShaderCacheHits(void)
{
    return __atomic_load_n(&hits, __ATOMIC_RELAXED);
}

bool mglShaderCacheVerify(void)
{
    const char *v = getenv("MGL_SHADER_CACHE_VERIFY");

    return v && strcmp(v, "0");
}

void mglHashBegin(MglHash *h, const char *kind)
{
    setUp();
    CC_SHA256_Init(&h->sha);
    mglHashStr(h, kind);
    mglHashBytes(h, build_id, sizeof build_id);
}

void mglHashBytes(MglHash *h, const void *p, size_t n)
{
    mglHashU32(h, (uint32_t)n);
    if (n)
        CC_SHA256_Update(&h->sha, p, (CC_LONG)n);
}

void mglHashU32(MglHash *h, uint32_t v)
{
    CC_SHA256_Update(&h->sha, &v, sizeof v);
}

void mglHashStr(MglHash *h, const char *s)
{
    mglHashU32(h, s != NULL);
    if (s)
        mglHashBytes(h, s, strlen(s));
}

void mglHashResource(MglHash *h, const void *resource)
{
    mglHashBytes(h, resource, offsetof(glslang_resource_t, limits) + sizeof(glslang_limits_t));
}

void mglHashEnd(MglHash *h, char hex[65])
{
    unsigned char d[CC_SHA256_DIGEST_LENGTH];

    CC_SHA256_Final(d, &h->sha);

    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++)
        snprintf(hex + i * 2, 3, "%02x", d[i]);
}

void mglBlobPut(MglBlob *b, const void *p, size_t n)
{
    if (b->bad)
        return;

    if (b->len + n > b->cap)
    {
        size_t cap = b->cap ? b->cap * 2 : 4096;

        while (cap < b->len + n)
            cap *= 2;

        unsigned char *grown = (unsigned char *)realloc(b->data, cap);

        if (grown == NULL)
        {
            b->bad = true;
            return;
        }

        b->data = grown;
        b->cap = cap;
    }

    if (n)
        memcpy(b->data + b->len, p, n);
    b->len += n;
}

void mglBlobPutU32(MglBlob *b, uint32_t v)
{
    mglBlobPut(b, &v, sizeof v);
}

void mglBlobPutStr(MglBlob *b, const char *s)
{
    mglBlobPutU32(b, s ? (uint32_t)strlen(s) + 1 : 0);
    if (s)
        mglBlobPut(b, s, strlen(s));
}

bool mglBlobGet(MglBlob *b, void *p, size_t n)
{
    if (b->bad || n > b->len - b->at)
    {
        b->bad = true;
        memset(p, 0, n);
        return false;
    }

    memcpy(p, b->data + b->at, n);
    b->at += n;
    return true;
}

uint32_t mglBlobGetU32(MglBlob *b)
{
    uint32_t v = 0;

    mglBlobGet(b, &v, sizeof v);
    return v;
}

char *mglBlobGetStr(MglBlob *b)
{
    uint32_t n = mglBlobGetU32(b);
    char *s;

    if (n == 0 || b->bad || n - 1 > b->len - b->at || (s = (char *)malloc(n)) == NULL)
    {
        if (n)
            b->bad = true;
        return NULL;
    }

    mglBlobGet(b, s, n - 1);
    s[n - 1] = 0;
    return s;
}

void mglBlobFree(MglBlob *b)
{
    free(b->data);
    memset(b, 0, sizeof *b);
}

static void pathFor(const char *kind, const char *hex, char *path, size_t n)
{
    snprintf(path, n, "%s/%s/%.2s/%s", cache_dir, kind, hex, hex + 2);
}

bool mglShaderCacheGet(const char *kind, const char *hex, MglBlob *out)
{
    char path[1200];
    struct stat st;
    int fd;

    memset(out, 0, sizeof *out);

    if (!mglShaderCacheOn())
        return false;

    pathFor(kind, hex, path, sizeof path);

    if ((fd = open(path, O_RDONLY)) < 0)
        return false;

    if (fstat(fd, &st) != 0 || st.st_size < (off_t)(sizeof magic + CC_SHA256_DIGEST_LENGTH))
    {
        close(fd);
        return false;
    }

    size_t size = (size_t)st.st_size;
    unsigned char *data = (unsigned char *)malloc(size);

    if (data == NULL || read(fd, data, size) != (ssize_t)size)
    {
        free(data);
        close(fd);
        return false;
    }

    close(fd);

    // a file cut short or changed since it was written is not used
    unsigned char sum[CC_SHA256_DIGEST_LENGTH];
    size_t head = sizeof magic + sizeof sum;

    CC_SHA256(data + head, (CC_LONG)(size - head), sum);

    if (memcmp(data, magic, sizeof magic) || memcmp(data + sizeof magic, sum, sizeof sum))
    {
        MGL_INFO("MGL: shader cache entry %s is damaged, making it again\n", path);
        free(data);
        return false;
    }

    // a use counts as recent, so trimming keeps what is still read
    utimes(path, NULL);
    __atomic_fetch_add(&hits, 1, __ATOMIC_RELAXED);

    out->data = data;
    out->len = size;
    out->cap = size;
    out->at = head;
    return true;
}

static void makeDirs(char *path)
{
    for (char *p = path + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(path, 0755);
            *p = '/';
        }
    }
}

void mglShaderCachePut(const char *kind, const char *hex, const MglBlob *b)
{
    char path[1200], tmp[1300];
    unsigned char sum[CC_SHA256_DIGEST_LENGTH];
    int fd;

    if (!mglShaderCacheOn() || b->bad)
        return;

    pathFor(kind, hex, path, sizeof path);
    makeDirs(path);

    // written whole under another name, then renamed, so a reader never sees half
    snprintf(tmp, sizeof tmp, "%s.XXXXXX", path);

    if ((fd = mkstemp(tmp)) < 0)
        return;

    CC_SHA256(b->data, (CC_LONG)b->len, sum);

    bool ok = write(fd, magic, sizeof magic) == (ssize_t)sizeof magic &&
              write(fd, sum, sizeof sum) == (ssize_t)sizeof sum &&
              write(fd, b->data, b->len) == (ssize_t)b->len;

    close(fd);

    if (!ok || rename(tmp, path) != 0)
        unlink(tmp);
}
