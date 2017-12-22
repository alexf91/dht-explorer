/*
 * Copyright 2017 Alexander Fasching
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/sha.h>


int dht_blacklisted(const struct sockaddr *sa, int salen)
{
    return 0;
}

void dht_hash(void *hash_return, int hash_size,
              const void *v1, int len1,
              const void *v2, int len2,
              const void *v3, int len3)
{
    SHA_CTX c;
    unsigned char md[SHA_DIGEST_LENGTH] = {0};

    SHA1_Init(&c);
    SHA1_Update(&c, v1, len1);
    SHA1_Update(&c, v2, len2);
    SHA1_Update(&c, v3, len3);
    SHA1_Final(md, &c);

    if(hash_size > SHA_DIGEST_LENGTH) {
        memcpy(hash_return, md, SHA_DIGEST_LENGTH);
        memset(hash_return + SHA_DIGEST_LENGTH, 0, hash_size - SHA_DIGEST_LENGTH);
    }
    else {
        memcpy(hash_return, md, hash_size);
    }
}

int dht_random_bytes(void *buf, size_t size)
{
    int fd, rc, save;

    fd = open("/dev/urandom", O_RDONLY);
    if(fd < 0)
        return -1;

    rc = read(fd, buf, size);

    save = errno;
    close(fd);
    errno = save;

    return rc;
}
