// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_HASHTABLE_H)
#define INCLUDED_HASHTABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"

typedef struct hashtable_entry_s
{
    const char *key;
    union
    {
        void     *value_ptr;
        uintptr_t value_int;
    };
} hashtable_entry_t;

typedef struct hashtable_s
{
    uint32_t mask;
    hashtable_entry_t *table;
} hashtable_t;

bool hashtable_init(hashtable_t *ht, uint init_size);

bool hashtable_add_entry(hashtable_t *ht, hashtable_entry_t entry);
hashtable_entry_t *hashtable_get_entry(const hashtable_t *ht, const char *key);

static inline bool hashtable_add(hashtable_t *ht, const char *key, void *value_ptr)
{
    return hashtable_add_entry(ht, (hashtable_entry_t){ .key = strdup(key), .value_ptr = value_ptr });
}

static inline void *hashtable_get(const hashtable_t *ht, const char *key)
{
    const hashtable_entry_t *entry = hashtable_get_entry(ht, key);
    return entry ? entry->value_ptr : 0;
}

#define HASHTABLE_ENTRY_NOT_FOUND (~(uint32_t)0)

static inline bool hashtable_addi(hashtable_t *ht, const char *key, uintptr_t value_int)
{
    return hashtable_add_entry(ht, (hashtable_entry_t){ .key = strdup(key), .value_int = value_int });
}

static inline uintptr_t hashtable_geti(const hashtable_t *ht, const char *key)
{
    const hashtable_entry_t *entry = hashtable_get_entry(ht, key);
    return entry ? entry->value_int : HASHTABLE_ENTRY_NOT_FOUND;
}

#endif // INCLUDED_HASHTABLE_H
