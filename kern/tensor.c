/*
 * GNU Mach Tensor Subsystem
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * Inspired by ATen (https://github.com/o9nn/ATen)
 * Lightweight tensor operations for kernel cognitive computing.
 */

#include <kern/tensor.h>
#include <kern/kalloc.h>
#include <kern/printf.h>
#include <kern/lock.h>
#include <kern/slab.h>
#include <string.h>

/*
 * Memory cache for tensor structures
 */
static struct kmem_cache tensor_cache;
static struct kmem_cache embedding_cache;
static boolean_t tensor_initialized = FALSE;

/*
 * Simple PRNG for embedding initialization
 */
static uint32_t tensor_rand_state = 12345;

static uint32_t tensor_rand(void)
{
    tensor_rand_state = tensor_rand_state * 1103515245 + 12345;
    return (tensor_rand_state >> 16) & 0x7FFF;
}

/*
 * Fixed-point square root approximation (Newton-Raphson)
 */
static tensor_scalar_t tensor_fixed_sqrt(tensor_scalar_t x)
{
    if (x <= 0)
        return 0;

    tensor_scalar_t guess = x >> 1;
    if (guess == 0)
        guess = 1;

    /* Newton-Raphson iterations */
    for (int i = 0; i < 8; i++) {
        tensor_scalar_t new_guess = (guess + TENSOR_FIXED_DIV(x, guess)) >> 1;
        if (new_guess >= guess)
            break;
        guess = new_guess;
    }

    return guess;
}

/*
 * Tensor subsystem initialization
 */
void tensor_subsystem_init(void)
{
    if (tensor_initialized)
        return;

    kmem_cache_init(&tensor_cache, "tensor",
                   sizeof(struct tensor), 0, NULL, 0);
    kmem_cache_init(&embedding_cache, "tensor_embedding",
                   sizeof(struct tensor_embedding), 0, NULL, 0);

    tensor_initialized = TRUE;
    printf("tensor: subsystem initialized (embed_dim=%d, fixed_point=%d bits)\n",
           TENSOR_EMBED_DIM, TENSOR_FIXED_POINT_BITS);
}

/*
 * Tensor creation
 */
kern_return_t tensor_create(tensor_t *result, uint32_t *shape,
                           uint16_t ndim, tensor_dtype_t dtype)
{
    tensor_t t;
    uint32_t numel = 1;
    uint32_t stride = 1;

    if (!result || !shape || ndim == 0 || ndim > TENSOR_MAX_DIMS)
        return TENSOR_INVALID_ARG;

    /* Calculate total elements */
    for (int i = 0; i < ndim; i++) {
        if (shape[i] == 0)
            return TENSOR_INVALID_ARG;
        numel *= shape[i];
    }

    /* Allocate tensor structure */
    t = (tensor_t)kmem_cache_alloc(&tensor_cache);
    if (!t)
        return TENSOR_NO_MEMORY;

    /* Allocate data */
    t->data = (tensor_scalar_t *)kalloc(numel * sizeof(tensor_scalar_t));
    if (!t->data) {
        kmem_cache_free(&tensor_cache, (vm_offset_t)t);
        return TENSOR_NO_MEMORY;
    }

    /* Initialize structure */
    t->ndim = ndim;
    t->dtype = dtype;
    t->numel = numel;
    t->refcount = 1;
    t->flags = TENSOR_FLAG_OWNS_DATA | TENSOR_FLAG_CONTIGUOUS;

    /* Set shape and calculate strides (row-major) */
    for (int i = ndim - 1; i >= 0; i--) {
        t->shape[i] = shape[i];
        t->strides[i] = stride;
        stride *= shape[i];
    }

    /* Zero remaining dimensions */
    for (int i = ndim; i < TENSOR_MAX_DIMS; i++) {
        t->shape[i] = 0;
        t->strides[i] = 0;
    }

    simple_lock_init(&t->lock);
    memset(t->data, 0, numel * sizeof(tensor_scalar_t));

    *result = t;
    return TENSOR_SUCCESS;
}

kern_return_t tensor_create_1d(tensor_t *result, uint32_t size)
{
    uint32_t shape[1] = { size };
    return tensor_create(result, shape, 1, TENSOR_DTYPE_FIXED16);
}

kern_return_t tensor_create_2d(tensor_t *result, uint32_t rows, uint32_t cols)
{
    uint32_t shape[2] = { rows, cols };
    return tensor_create(result, shape, 2, TENSOR_DTYPE_FIXED16);
}

kern_return_t tensor_create_from_data(tensor_t *result, tensor_scalar_t *data,
                                      uint32_t *shape, uint16_t ndim)
{
    kern_return_t kr;
    tensor_t t;

    kr = tensor_create(&t, shape, ndim, TENSOR_DTYPE_FIXED16);
    if (kr != TENSOR_SUCCESS)
        return kr;

    memcpy(t->data, data, t->numel * sizeof(tensor_scalar_t));
    *result = t;
    return TENSOR_SUCCESS;
}

/*
 * Tensor destruction
 */
void tensor_destroy(tensor_t t)
{
    if (!t)
        return;

    if (t->flags & TENSOR_FLAG_OWNS_DATA && t->data) {
        kfree((vm_offset_t)t->data, t->numel * sizeof(tensor_scalar_t));
    }

    kmem_cache_free(&tensor_cache, (vm_offset_t)t);
}

void tensor_retain(tensor_t t)
{
    if (t)
        __sync_add_and_fetch(&t->refcount, 1);
}

void tensor_release(tensor_t t)
{
    if (t && __sync_sub_and_fetch(&t->refcount, 1) == 0)
        tensor_destroy(t);
}

/*
 * Element access
 */
tensor_scalar_t tensor_get_1d(tensor_t t, uint32_t i)
{
    if (!t || i >= t->numel)
        return 0;
    return t->data[i];
}

tensor_scalar_t tensor_get_2d(tensor_t t, uint32_t i, uint32_t j)
{
    if (!t || t->ndim < 2 || i >= t->shape[0] || j >= t->shape[1])
        return 0;
    return t->data[i * t->strides[0] + j * t->strides[1]];
}

void tensor_set_1d(tensor_t t, uint32_t i, tensor_scalar_t val)
{
    if (t && i < t->numel && !(t->flags & TENSOR_FLAG_READONLY))
        t->data[i] = val;
}

void tensor_set_2d(tensor_t t, uint32_t i, uint32_t j, tensor_scalar_t val)
{
    if (t && t->ndim >= 2 && i < t->shape[0] && j < t->shape[1] &&
        !(t->flags & TENSOR_FLAG_READONLY))
        t->data[i * t->strides[0] + j * t->strides[1]] = val;
}

/*
 * Arithmetic operations
 */
kern_return_t tensor_add(tensor_t result, tensor_t a, tensor_t b)
{
    if (!result || !a || !b)
        return TENSOR_INVALID_ARG;
    if (result->numel != a->numel || a->numel != b->numel)
        return TENSOR_SHAPE_MISMATCH;

    for (uint32_t i = 0; i < result->numel; i++)
        result->data[i] = a->data[i] + b->data[i];

    return TENSOR_SUCCESS;
}

kern_return_t tensor_sub(tensor_t result, tensor_t a, tensor_t b)
{
    if (!result || !a || !b)
        return TENSOR_INVALID_ARG;
    if (result->numel != a->numel || a->numel != b->numel)
        return TENSOR_SHAPE_MISMATCH;

    for (uint32_t i = 0; i < result->numel; i++)
        result->data[i] = a->data[i] - b->data[i];

    return TENSOR_SUCCESS;
}

kern_return_t tensor_mul_scalar(tensor_t result, tensor_t a, tensor_scalar_t s)
{
    if (!result || !a)
        return TENSOR_INVALID_ARG;
    if (result->numel != a->numel)
        return TENSOR_SHAPE_MISMATCH;

    for (uint32_t i = 0; i < result->numel; i++)
        result->data[i] = TENSOR_FIXED_MUL(a->data[i], s);

    return TENSOR_SUCCESS;
}

kern_return_t tensor_elementwise_mul(tensor_t result, tensor_t a, tensor_t b)
{
    if (!result || !a || !b)
        return TENSOR_INVALID_ARG;
    if (result->numel != a->numel || a->numel != b->numel)
        return TENSOR_SHAPE_MISMATCH;

    for (uint32_t i = 0; i < result->numel; i++)
        result->data[i] = TENSOR_FIXED_MUL(a->data[i], b->data[i]);

    return TENSOR_SUCCESS;
}

/*
 * Reduction operations
 */
tensor_scalar_t tensor_sum(tensor_t t)
{
    tensor_scalar_t sum = 0;
    if (!t)
        return 0;

    for (uint32_t i = 0; i < t->numel; i++)
        sum += t->data[i];

    return sum;
}

tensor_scalar_t tensor_mean(tensor_t t)
{
    if (!t || t->numel == 0)
        return 0;
    return TENSOR_FIXED_DIV(tensor_sum(t), TENSOR_TO_FIXED(t->numel));
}

tensor_scalar_t tensor_max(tensor_t t)
{
    tensor_scalar_t max_val;
    if (!t || t->numel == 0)
        return 0;

    max_val = t->data[0];
    for (uint32_t i = 1; i < t->numel; i++)
        if (t->data[i] > max_val)
            max_val = t->data[i];

    return max_val;
}

tensor_scalar_t tensor_min(tensor_t t)
{
    tensor_scalar_t min_val;
    if (!t || t->numel == 0)
        return 0;

    min_val = t->data[0];
    for (uint32_t i = 1; i < t->numel; i++)
        if (t->data[i] < min_val)
            min_val = t->data[i];

    return min_val;
}

/*
 * Linear algebra
 */
kern_return_t tensor_dot(tensor_scalar_t *result, tensor_t a, tensor_t b)
{
    tensor_scalar_t sum = 0;

    if (!result || !a || !b)
        return TENSOR_INVALID_ARG;
    if (a->numel != b->numel)
        return TENSOR_SHAPE_MISMATCH;

    for (uint32_t i = 0; i < a->numel; i++)
        sum += TENSOR_FIXED_MUL(a->data[i], b->data[i]);

    *result = sum;
    return TENSOR_SUCCESS;
}

kern_return_t tensor_matmul(tensor_t result, tensor_t a, tensor_t b)
{
    if (!result || !a || !b)
        return TENSOR_INVALID_ARG;
    if (a->ndim != 2 || b->ndim != 2 || result->ndim != 2)
        return TENSOR_INVALID_ARG;
    if (a->shape[1] != b->shape[0])
        return TENSOR_SHAPE_MISMATCH;
    if (result->shape[0] != a->shape[0] || result->shape[1] != b->shape[1])
        return TENSOR_SHAPE_MISMATCH;

    uint32_t M = a->shape[0];
    uint32_t K = a->shape[1];
    uint32_t N = b->shape[1];

    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            tensor_scalar_t sum = 0;
            for (uint32_t k = 0; k < K; k++) {
                sum += TENSOR_FIXED_MUL(
                    tensor_get_2d(a, i, k),
                    tensor_get_2d(b, k, j)
                );
            }
            tensor_set_2d(result, i, j, sum);
        }
    }

    return TENSOR_SUCCESS;
}

kern_return_t tensor_normalize(tensor_t result, tensor_t a)
{
    tensor_scalar_t norm_sq = 0;
    tensor_scalar_t norm;

    if (!result || !a)
        return TENSOR_INVALID_ARG;
    if (result->numel != a->numel)
        return TENSOR_SHAPE_MISMATCH;

    /* Calculate L2 norm squared */
    for (uint32_t i = 0; i < a->numel; i++)
        norm_sq += TENSOR_FIXED_MUL(a->data[i], a->data[i]);

    norm = tensor_fixed_sqrt(norm_sq);
    if (norm == 0)
        norm = TENSOR_FIXED_POINT_ONE;  /* Avoid division by zero */

    for (uint32_t i = 0; i < a->numel; i++)
        result->data[i] = TENSOR_FIXED_DIV(a->data[i], norm);

    return TENSOR_SUCCESS;
}

/*
 * Embedding operations
 */
void tensor_embedding_init(tensor_embedding_t e, uint32_t entity_id, uint16_t dim)
{
    if (!e)
        return;

    memset(e, 0, sizeof(struct tensor_embedding));
    e->entity_id = entity_id;
    e->dim = (dim > TENSOR_EMBED_DIM) ? TENSOR_EMBED_DIM : dim;
    e->timestamp = 0;
    e->flags = EMBED_FLAG_VALID;
}

void tensor_embedding_zero(tensor_embedding_t e)
{
    if (!e)
        return;
    memset(e->values, 0, sizeof(e->values));
    e->flags &= ~EMBED_FLAG_NORMALIZED;
}

void tensor_embedding_random(tensor_embedding_t e, uint32_t seed)
{
    if (!e)
        return;

    tensor_rand_state = seed;

    for (int i = 0; i < e->dim; i++) {
        /* Random value in range [-0.5, 0.5] in fixed point */
        int32_t r = tensor_rand();
        e->values[i] = (tensor_scalar_t)(r - 16384) << (TENSOR_FIXED_POINT_BITS - 15);
    }

    e->flags &= ~EMBED_FLAG_NORMALIZED;
    tensor_embedding_normalize(e);
}

kern_return_t tensor_embedding_normalize(tensor_embedding_t e)
{
    tensor_scalar_t norm_sq = 0;
    tensor_scalar_t norm;

    if (!e)
        return TENSOR_INVALID_ARG;

    for (int i = 0; i < e->dim; i++)
        norm_sq += TENSOR_FIXED_MUL(e->values[i], e->values[i]);

    norm = tensor_fixed_sqrt(norm_sq);
    if (norm == 0) {
        e->flags &= ~EMBED_FLAG_NORMALIZED;
        return TENSOR_SUCCESS;
    }

    for (int i = 0; i < e->dim; i++)
        e->values[i] = TENSOR_FIXED_DIV(e->values[i], norm);

    e->flags |= EMBED_FLAG_NORMALIZED;
    return TENSOR_SUCCESS;
}

tensor_scalar_t tensor_embedding_dot(tensor_embedding_t a, tensor_embedding_t b)
{
    tensor_scalar_t sum = 0;
    int min_dim;

    if (!a || !b)
        return 0;

    min_dim = (a->dim < b->dim) ? a->dim : b->dim;

    for (int i = 0; i < min_dim; i++)
        sum += TENSOR_FIXED_MUL(a->values[i], b->values[i]);

    return sum;
}

tensor_scalar_t tensor_embedding_cosine_sim(tensor_embedding_t a, tensor_embedding_t b)
{
    tensor_scalar_t dot_prod;
    tensor_scalar_t norm_a_sq = 0, norm_b_sq = 0;
    tensor_scalar_t norm_a, norm_b, norm_prod;
    int min_dim;

    if (!a || !b)
        return 0;

    /* If both are normalized, dot product is cosine similarity */
    if ((a->flags & EMBED_FLAG_NORMALIZED) && (b->flags & EMBED_FLAG_NORMALIZED))
        return tensor_embedding_dot(a, b);

    min_dim = (a->dim < b->dim) ? a->dim : b->dim;

    dot_prod = 0;
    for (int i = 0; i < min_dim; i++) {
        dot_prod += TENSOR_FIXED_MUL(a->values[i], b->values[i]);
        norm_a_sq += TENSOR_FIXED_MUL(a->values[i], a->values[i]);
        norm_b_sq += TENSOR_FIXED_MUL(b->values[i], b->values[i]);
    }

    norm_a = tensor_fixed_sqrt(norm_a_sq);
    norm_b = tensor_fixed_sqrt(norm_b_sq);

    if (norm_a == 0 || norm_b == 0)
        return 0;

    norm_prod = TENSOR_FIXED_MUL(norm_a, norm_b);
    return TENSOR_FIXED_DIV(dot_prod, norm_prod);
}

tensor_scalar_t tensor_embedding_l2_distance(tensor_embedding_t a, tensor_embedding_t b)
{
    tensor_scalar_t sum = 0;
    int min_dim;

    if (!a || !b)
        return 0;

    min_dim = (a->dim < b->dim) ? a->dim : b->dim;

    for (int i = 0; i < min_dim; i++) {
        tensor_scalar_t diff = a->values[i] - b->values[i];
        sum += TENSOR_FIXED_MUL(diff, diff);
    }

    return tensor_fixed_sqrt(sum);
}

void tensor_embedding_add(tensor_embedding_t result,
                         tensor_embedding_t a, tensor_embedding_t b)
{
    int min_dim;

    if (!result || !a || !b)
        return;

    min_dim = (a->dim < b->dim) ? a->dim : b->dim;
    result->dim = min_dim;

    for (int i = 0; i < min_dim; i++)
        result->values[i] = a->values[i] + b->values[i];

    result->flags = EMBED_FLAG_VALID;
}

void tensor_embedding_scale(tensor_embedding_t result,
                           tensor_embedding_t a, tensor_scalar_t s)
{
    if (!result || !a)
        return;

    result->dim = a->dim;

    for (int i = 0; i < a->dim; i++)
        result->values[i] = TENSOR_FIXED_MUL(a->values[i], s);

    result->flags = EMBED_FLAG_VALID;
}

void tensor_embedding_lerp(tensor_embedding_t result,
                          tensor_embedding_t a, tensor_embedding_t b,
                          tensor_scalar_t t)
{
    tensor_scalar_t one_minus_t = TENSOR_FIXED_POINT_ONE - t;
    int min_dim;

    if (!result || !a || !b)
        return;

    min_dim = (a->dim < b->dim) ? a->dim : b->dim;
    result->dim = min_dim;

    for (int i = 0; i < min_dim; i++) {
        result->values[i] = TENSOR_FIXED_MUL(a->values[i], one_minus_t) +
                           TENSOR_FIXED_MUL(b->values[i], t);
    }

    result->flags = EMBED_FLAG_VALID;
}
