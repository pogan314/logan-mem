#ifndef LSM_SHA256_H
#define LSM_SHA256_H

/* In-process SHA-256 (FIPS 180-4). Used to verify the integrity of a
 * downloaded release before installing it, without shelling out to a
 * platform hashing tool (shasum / sha256sum / certutil) — those differ per
 * OS, may be absent, and mis-quote paths under cmd.exe. */

#include <stddef.h>
#include <stdint.h>

#define LSM_SHA256_DIGEST_LEN 32 /* raw digest bytes */
#define LSM_SHA256_HEX_LEN 64    /* lowercase hex chars (no NUL) */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buf[64];
    size_t buflen;
} lsm_sha256_ctx;

void lsm_sha256_init(lsm_sha256_ctx *c);
void lsm_sha256_update(lsm_sha256_ctx *c, const void *data, size_t len);
void lsm_sha256_final(lsm_sha256_ctx *c, uint8_t out[LSM_SHA256_DIGEST_LEN]);

/* One-shot hash of a buffer to lowercase hex. `out` must hold
 * LSM_SHA256_HEX_LEN + 1 bytes (hex chars + NUL). */
void lsm_sha256_hex(const void *data, size_t len, char out[LSM_SHA256_HEX_LEN + 1]);

/* RFC 2104 HMAC-SHA-256. The output is always LSM_SHA256_DIGEST_LEN bytes.
 * A NULL key/data pointer is accepted only when its corresponding length is
 * zero. */
void lsm_hmac_sha256(const void *key, size_t key_len, const void *data, size_t data_len,
                     uint8_t out[LSM_SHA256_DIGEST_LEN]);

#endif /* LSM_SHA256_H */
