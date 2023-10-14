// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "hashtable.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static uint32_t fnv1a_add_bytes(const char *key, uint32_t hash)
{

    return hash;
}

static inline uint32_t fnv1a(const char *key)
{
    uint32_t hash = UINT32_C(0x811c9dc5);
    while (*key)
    {
        hash = (hash ^ (uint32_t)*key) * UINT32_C(0x01000193);
        ++key;
    }
    return hash;
}

//static inline uint64_t fnv1a64(const char *key)
//{
//    uint64_t hash = UINT64_C(0xcbf29ce484222325);
//    while (*key)
//    {
//        hash = (hash ^ (int64_t)*key) * UINT64_C(0x00000100000001B3);
//        ++key;
//    }
//    return hash;
//}


static uint32_t clog2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bool hashtable_init(hashtable_t *ht, uint init_size)
{
    init_size = clog2(init_size);
    ht->mask  = init_size-1;
    ht->table = calloc(init_size, sizeof(hashtable_entry_t));
    return ht->table != 0;
}

static hashtable_entry_t *find_entry(const hashtable_t *ht, const char *key)
{
    uint32_t hash = fnv1a(key);
    for (uint i=1; i<=20; ++i)
    {
        uint bucket = hash & ht->mask;
        if (!ht->table[bucket].key)
            return &ht->table[bucket];
        else if (strcmp(ht->table[bucket].key, key) == 0)
            return &ht->table[bucket];

        hash += i;
    }
    return 0;
}

static bool copy_ht(hashtable_t *nht, hashtable_t *ht)
{
    for (uint i=0; i<=ht->mask; ++i)
    {
        if (!ht->table[i].key)
            continue;

        hashtable_entry_t *e = find_entry(nht, ht->table[i].key);
        if (e && !e->key)
            *e = ht->table[i];
        else
            return false; // too many probes
    }
    return true;
}

bool hashtable_add_entry(hashtable_t *ht, hashtable_entry_t entry)
{
    hashtable_entry_t *e = find_entry(ht, entry.key);
    if (e) // slot found
    {
        if (!e->key) // slot empty
        {
            *e = entry;
            return true;
        }
        else // slot full (dup)
            return false;
    }

    uint new_size = clog2(ht->mask + 2);
    hashtable_t nht;

    for (uint i=0; i<8; ++i)
    {
        if (!hashtable_init(&nht, new_size))
            return false;

        if (copy_ht(&nht, ht))
        {
            e = find_entry(&nht, entry.key);
            if (e)
            {
                free(ht->table);
                *ht = nht;

                if(!e->key) // slot empty
                {
                    *e = entry;
                    return true;
                }
                else // slot full (dup)
                    return false;
            }
        }

        free(nht.table);
        new_size *= 2;
    }

    return false;
}

hashtable_entry_t *hashtable_get_entry(const hashtable_t *ht, const char *key)
{
    hashtable_entry_t *e = find_entry(ht, key);
    return e && e->key ? e : 0;
}
