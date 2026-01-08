/*
 * GNU Mach Tensor Subsystem
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * GNU Mach is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

/*
 * tensor.h - Lightweight Tensor Library for Kernel
 *
 * Inspired by ATen (https://github.com/o9nn/ATen), this provides
 * tensor operations for kernel-level cognitive computing.
 *
 * Design Goals:
 *   - Fixed-point arithmetic (no FPU in kernel)
 *   - Small, fixed-dimension tensors for embeddings
 *   - Lock-free operations where possible
 *   - Memory-efficient representations
 */

#ifndef _KERN_TENSOR_H_
#define _KERN_TENSOR_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <kern/lock.h>

/*
 * Tensor configuration constants
 */
#define TENSOR_MAX_DIMS         4       /* Maximum tensor dimensions */
#define TENSOR_EMBED_DIM        16      /* Default embedding dimension */
#define TENSOR_EMBED_DIM_LARGE  64      /* Large embedding dimension */
#define TENSOR_FIXED_POINT_BITS 16      /* Fixed-point fractional bits */
#define TENSOR_FIXED_POINT_ONE  (1 << TENSOR_FIXED_POINT_BITS)

/*
 * Fixed-point arithmetic type
 * Using 32-bit integers with 16 fractional bits gives us
 * range [-32768, 32767] with precision of ~0.00001
 */
typedef int32_t tensor_scalar_t;

/*
 * Convert between fixed-point and integer
 */
#define TENSOR_TO_FIXED(x)      ((tensor_scalar_t)((x) << TENSOR_FIXED_POINT_BITS))
#define TENSOR_FROM_FIXED(x)    ((x) >> TENSOR_FIXED_POINT_BITS)
#define TENSOR_FIXED_MUL(a, b)  (((int64_t)(a) * (b)) >> TENSOR_FIXED_POINT_BITS)
#define TENSOR_FIXED_DIV(a, b)  (((int64_t)(a) << TENSOR_FIXED_POINT_BITS) / (b))

/*
 * Tensor data types
 */
typedef enum {
    TENSOR_DTYPE_INT32,         /* 32-bit signed integer */
    TENSOR_DTYPE_FIXED16,       /* 16.16 fixed-point */
    TENSOR_DTYPE_UINT8,         /* 8-bit unsigned (for quantized) */
} tensor_dtype_t;

/*
 * Tensor storage layout
 */
typedef enum {
    TENSOR_LAYOUT_CONTIGUOUS,   /* Row-major contiguous */
    TENSOR_LAYOUT_STRIDED,      /* Strided access pattern */
} tensor_layout_t;

/*
 * Core tensor structure
 * Designed to be small and cache-friendly
 */
struct tensor {
    tensor_scalar_t     *data;          /* Data pointer */
    uint32_t            shape[TENSOR_MAX_DIMS]; /* Dimension sizes */
    uint32_t            strides[TENSOR_MAX_DIMS]; /* Access strides */
    uint16_t            ndim;           /* Number of dimensions */
    uint16_t            flags;          /* Tensor flags */
    tensor_dtype_t      dtype;          /* Data type */
    uint32_t            numel;          /* Total number of elements */
    uint32_t            refcount;       /* Reference count */
    decl_simple_lock_data(, lock)       /* Tensor lock */
};

typedef struct tensor *tensor_t;

/*
 * Tensor flags
 */
#define TENSOR_FLAG_OWNS_DATA   0x0001  /* Tensor owns its data */
#define TENSOR_FLAG_CONTIGUOUS  0x0002  /* Data is contiguous */
#define TENSOR_FLAG_READONLY    0x0004  /* Read-only tensor */
#define TENSOR_FLAG_PINNED      0x0008  /* Pinned in memory */

/*
 * Embedding vector - fixed-size for efficiency
 * Used for entity representations in kernel
 */
struct tensor_embedding {
    tensor_scalar_t     values[TENSOR_EMBED_DIM];
    uint32_t            entity_id;      /* Associated entity */
    uint32_t            timestamp;      /* Last update time */
    uint16_t            dim;            /* Actual dimension used */
    uint16_t            flags;          /* Embedding flags */
};

typedef struct tensor_embedding *tensor_embedding_t;

/*
 * Embedding flags
 */
#define EMBED_FLAG_VALID        0x0001  /* Embedding is valid */
#define EMBED_FLAG_NORMALIZED   0x0002  /* Normalized to unit length */
#define EMBED_FLAG_CACHED       0x0004  /* Cached computation result */

/*
 * Tensor operations result codes
 */
#define TENSOR_SUCCESS          KERN_SUCCESS
#define TENSOR_INVALID_ARG      KERN_INVALID_ARGUMENT
#define TENSOR_NO_MEMORY        KERN_RESOURCE_SHORTAGE
#define TENSOR_SHAPE_MISMATCH   KERN_FAILURE

/*
 * Tensor creation and destruction
 */
kern_return_t tensor_create(tensor_t *result, uint32_t *shape,
                           uint16_t ndim, tensor_dtype_t dtype);
kern_return_t tensor_create_1d(tensor_t *result, uint32_t size);
kern_return_t tensor_create_2d(tensor_t *result, uint32_t rows, uint32_t cols);
kern_return_t tensor_create_from_data(tensor_t *result, tensor_scalar_t *data,
                                      uint32_t *shape, uint16_t ndim);
void tensor_destroy(tensor_t t);
void tensor_retain(tensor_t t);
void tensor_release(tensor_t t);

/*
 * Tensor element access
 */
tensor_scalar_t tensor_get_1d(tensor_t t, uint32_t i);
tensor_scalar_t tensor_get_2d(tensor_t t, uint32_t i, uint32_t j);
void tensor_set_1d(tensor_t t, uint32_t i, tensor_scalar_t val);
void tensor_set_2d(tensor_t t, uint32_t i, uint32_t j, tensor_scalar_t val);

/*
 * Tensor arithmetic operations
 */
kern_return_t tensor_add(tensor_t result, tensor_t a, tensor_t b);
kern_return_t tensor_sub(tensor_t result, tensor_t a, tensor_t b);
kern_return_t tensor_mul_scalar(tensor_t result, tensor_t a, tensor_scalar_t s);
kern_return_t tensor_elementwise_mul(tensor_t result, tensor_t a, tensor_t b);

/*
 * Tensor reduction operations
 */
tensor_scalar_t tensor_sum(tensor_t t);
tensor_scalar_t tensor_mean(tensor_t t);
tensor_scalar_t tensor_max(tensor_t t);
tensor_scalar_t tensor_min(tensor_t t);

/*
 * Tensor linear algebra
 */
kern_return_t tensor_dot(tensor_scalar_t *result, tensor_t a, tensor_t b);
kern_return_t tensor_matmul(tensor_t result, tensor_t a, tensor_t b);
kern_return_t tensor_normalize(tensor_t result, tensor_t a);

/*
 * Embedding operations
 */
void tensor_embedding_init(tensor_embedding_t e, uint32_t entity_id, uint16_t dim);
void tensor_embedding_zero(tensor_embedding_t e);
void tensor_embedding_random(tensor_embedding_t e, uint32_t seed);
kern_return_t tensor_embedding_normalize(tensor_embedding_t e);

/*
 * Embedding similarity metrics
 */
tensor_scalar_t tensor_embedding_dot(tensor_embedding_t a, tensor_embedding_t b);
tensor_scalar_t tensor_embedding_cosine_sim(tensor_embedding_t a, tensor_embedding_t b);
tensor_scalar_t tensor_embedding_l2_distance(tensor_embedding_t a, tensor_embedding_t b);

/*
 * Embedding arithmetic
 */
void tensor_embedding_add(tensor_embedding_t result,
                         tensor_embedding_t a, tensor_embedding_t b);
void tensor_embedding_scale(tensor_embedding_t result,
                           tensor_embedding_t a, tensor_scalar_t s);
void tensor_embedding_lerp(tensor_embedding_t result,
                          tensor_embedding_t a, tensor_embedding_t b,
                          tensor_scalar_t t);

/*
 * Tensor memory pool for kernel allocations
 */
void tensor_pool_init(void);
void *tensor_pool_alloc(uint32_t size);
void tensor_pool_free(void *ptr, uint32_t size);

/*
 * Tensor subsystem initialization
 */
void tensor_subsystem_init(void);

#endif /* _KERN_TENSOR_H_ */
