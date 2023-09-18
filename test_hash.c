// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include <stdio.h>
#include <string.h>

#include "hashtable.h"
#include "utils.h"


int main(int argc, char *argv[])
{
    hashtable_t ht;
    hashtable_init(&ht, 16);

    FILE *f = fopen(argv[1], "rb");
    char buffer[128];
    uint i = 0;
    while (fgets(buffer, sizeof(buffer), f))
        hashtable_addi(&ht, buffer, i++);

    fclose(f);
    printf("done reading, hash size now %d / %d\n", i, ht.mask+1);

    f = fopen(argv[1], "rb");
    i = 0;
    while (fgets(buffer, sizeof(buffer), f))
    {
        if (i != hashtable_geti(&ht, buffer))
        {
            printf("ACK! failed %s @ %i", buffer, i);
            return 1;
        }
        ++i;
    }

    return 0;
}
