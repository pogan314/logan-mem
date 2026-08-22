/*
 * rotsq.h — RaBitQ-style B-bit vector quantization (from-paper, plain C11).
 *
 * Implements the core of Extended RaBitQ (Gao et al., SIGMOD 2024/2025;
 * arXiv:2405.12497, arXiv:2409.09913) without the reference library (which is
 * C++ and bundles the Eigen linear-algebra dependency; nothing here is vendored
 * or derived from it — this file is written from the papers). Named rotsq, not
 * rabitq: this is the
 * FAMILY core (randomized rotation + per-vector scalar quantization + exact
 * code-expansion estimator), not the papers' exact codebook construction —
 * the name should not overclaim fidelity. Rotate each vector with a deterministic
 * randomized transform, then scalar-quantize the rotated coordinates to B
 * bits. The rotation spreads a vector's mass evenly across coordinates
 * (rotated coords of a unit vector are near-Gaussian), which makes plain
 * scalar quantization behave near-optimally — that observation IS RaBitQ.
 *
 * Rotation: random ±1 diagonal (XXH3-seeded, reproducible — same idiom as the
 * LSH hyperplanes) followed by a Fast Walsh–Hadamard Transform, inputs padded
 * to the next power of two (768 → 1024). Orthogonal up to a known 1/√D scale.
 *
 * Inner product between two ENCODED vectors is estimated from the codes with
 * the exact SQ expansion (deterministic — a pure function of the two codes):
 *   x_i ≈ ox + sx·cx_i  ⇒  ⟨x,y⟩ ≈ D·ox·oy + ox·sy·Σcy + oy·sx·Σcx
 *                                   + sx·sy·Σ(cx_i·cy_i)
 * with Σcx/Σcy precomputed per vector, so scoring one pair is one integer
 * dot product (u8×u8) plus four multiplies.
 *
 * Memory at B=4, D=1024: 512 B codes + 12 B metadata per vector, vs 3 072 B
 * for 768 raw floats — ~6× per vector. Cosine of normalized inputs is the
 * estimated IP directly (rotation preserves norms up to the fixed scale,
 * which cancels in the scale factors).
 */
#ifndef LSM_SEMANTIC_ROTSQ_H
#define LSM_SEMANTIC_ROTSQ_H

#include <stdint.h>

enum {
    LSM_RSQ_IN_DIM = 768,                 /* input dimension (LSM_SEM_DIM) */
    LSM_RSQ_DIM = 1024,                   /* padded pow2 rotation dimension */
    LSM_RSQ_BITS = 4,                     /* bits per coordinate */
    LSM_RSQ_LEVELS = 15,                  /* (1 << LSM_RSQ_BITS) - 1 */
    LSM_RSQ_CODE_BYTES = LSM_RSQ_DIM / 2, /* two 4-bit codes per byte */
};

typedef struct {
    uint8_t codes[LSM_RSQ_CODE_BYTES]; /* packed 4-bit codes, little nibble first */
    float scale;                       /* per-vector dequant scale  */
    float offset;                      /* per-vector dequant offset */
    int32_t code_sum;                  /* Σ codes (for the IP expansion) */
} lsm_rsq_code_t;

/* Encode a LSM_RSQ_IN_DIM float vector (zero-padded to LSM_RSQ_DIM, rotated,
 * quantized). Deterministic: the rotation is fixed at build time. */
void lsm_rsq_encode(const float *v, lsm_rsq_code_t *out);

/* Estimated inner product of the two ORIGINAL vectors from their codes.
 * Deterministic pure function of the codes. */
float lsm_rsq_ip(const lsm_rsq_code_t *a, const lsm_rsq_code_t *b);

/* Dequantize a code into the ROTATED space (LSM_RSQ_DIM floats). Note: this
 * is the rotated basis, not the original one — fine for basis-agnostic
 * consumers (LSH hyperplane signs), wrong for anything expecting original
 * coordinates. */
void lsm_rsq_decode(const lsm_rsq_code_t *c, float *out);

#endif /* LSM_SEMANTIC_ROTSQ_H */
