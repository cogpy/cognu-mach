/*
 * GNU Mach AtomSpace-Inspired Hypergraph Subsystem
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * Inspired by OpenCog AtomSpace and ATenSpace.
 * Provides hypergraph knowledge representation for kernel entities.
 */

#include <kern/atomspace.h>
#include <kern/kalloc.h>
#include <kern/printf.h>
#include <kern/slab.h>
#include <string.h>

/*
 * Memory cache for atomspaces
 */
static struct kmem_cache atomspace_cache;
static boolean_t atomspace_initialized = FALSE;

/*
 * Hash function for atom names
 */
static uint32_t atom_hash_name(const char *name, atom_type_t type)
{
    uint32_t hash = 5381;
    const char *p = name;

    while (*p) {
        hash = ((hash << 5) + hash) + *p++;
    }

    return (hash ^ (type << 16)) % ATOM_HASH_BUCKETS;
}

/*
 * Hash function for link outgoing set
 */
static uint32_t atom_hash_link(atom_handle_t *outgoing, uint8_t arity,
                               atom_type_t type)
{
    uint32_t hash = type;

    for (int i = 0; i < arity; i++) {
        hash = hash * 31 + outgoing[i];
    }

    return hash % ATOM_HASH_BUCKETS;
}

/*
 * Subsystem initialization
 */
void atomspace_subsystem_init(void)
{
    if (atomspace_initialized)
        return;

    kmem_cache_init(&atomspace_cache, "atomspace",
                   sizeof(struct atomspace), 0, NULL, 0);

    atomspace_initialized = TRUE;
    printf("atomspace: subsystem initialized (max_atoms=%d)\n",
           ATOMSPACE_MAX_ATOMS);
}

/*
 * AtomSpace creation
 */
kern_return_t atomspace_create(atomspace_t *result)
{
    atomspace_t as;

    if (!result)
        return KERN_INVALID_ARGUMENT;

    as = (atomspace_t)kmem_cache_alloc(&atomspace_cache);
    if (!as)
        return KERN_RESOURCE_SHORTAGE;

    memset(as, 0, sizeof(struct atomspace));

    /* Initialize hash table */
    for (int i = 0; i < ATOM_HASH_BUCKETS; i++)
        queue_init(&as->hash_table[i]);

    /* Initialize type index */
    for (int i = 0; i < 128; i++)
        queue_init(&as->type_index[i]);

    as->next_handle = 1;    /* 0 is invalid handle */
    as->attention_funds = 1000;
    as->wage = 10;
    as->version = 1;

    simple_lock_init(&as->lock);

    *result = as;
    return KERN_SUCCESS;
}

/*
 * AtomSpace destruction
 */
void atomspace_destroy(atomspace_t as)
{
    if (!as)
        return;

    atomspace_clear(as);
    kmem_cache_free(&atomspace_cache, (vm_offset_t)as);
}

void atomspace_clear(atomspace_t as)
{
    if (!as)
        return;

    simple_lock(&as->lock);

    /* Reset all atoms */
    for (uint32_t i = 0; i < as->atom_count; i++) {
        as->atoms[i].flags = 0;
        as->atoms[i].handle = ATOM_HANDLE_INVALID;
    }

    /* Clear hash table */
    for (int i = 0; i < ATOM_HASH_BUCKETS; i++)
        queue_init(&as->hash_table[i]);

    /* Clear type index */
    for (int i = 0; i < 128; i++)
        queue_init(&as->type_index[i]);

    as->atom_count = 0;
    as->node_count = 0;
    as->link_count = 0;
    as->next_handle = 1;

    simple_unlock(&as->lock);
}

/*
 * Find free atom slot
 */
static atom_t atomspace_alloc_atom(atomspace_t as)
{
    /* First try to find a removed slot */
    for (uint32_t i = 0; i < as->atom_count; i++) {
        if (as->atoms[i].flags & ATOM_FLAG_REMOVED) {
            as->atoms[i].flags = 0;
            return &as->atoms[i];
        }
    }

    /* Allocate new slot */
    if (as->atom_count >= ATOMSPACE_MAX_ATOMS)
        return NULL;

    return &as->atoms[as->atom_count++];
}

/*
 * Add node
 */
atom_handle_t atomspace_add_node(atomspace_t as, atom_type_t type,
                                 const char *name)
{
    atom_t a;
    atom_handle_t existing;
    uint32_t hash;

    if (!as || !name || !ATOM_IS_NODE(type))
        return ATOM_HANDLE_INVALID;

    simple_lock(&as->lock);

    /* Check if node already exists */
    existing = atomspace_find_node(as, type, name);
    if (existing != ATOM_HANDLE_INVALID) {
        simple_unlock(&as->lock);
        return existing;
    }

    /* Allocate new atom */
    a = atomspace_alloc_atom(as);
    if (!a) {
        simple_unlock(&as->lock);
        return ATOM_HANDLE_INVALID;
    }

    /* Initialize atom */
    memset(a, 0, sizeof(struct atom));
    a->handle = as->next_handle++;
    a->type = type;
    strncpy(a->name, name, ATOM_MAX_NAME_LEN - 1);
    a->name[ATOM_MAX_NAME_LEN - 1] = '\0';
    a->arity = 0;
    a->flags = ATOM_FLAG_VALID;
    a->refcount = 1;

    /* Default truth value */
    a->tv.strength = TV_TRUE_STRENGTH;
    a->tv.confidence = TV_DEFAULT_CONF;

    /* Default attention */
    a->av.sti = AV_DEFAULT_STI;
    a->av.lti = AV_DEFAULT_LTI;

    /* Initialize embedding */
    tensor_embedding_init(&a->embedding, a->handle, TENSOR_EMBED_DIM);

    simple_lock_init(&a->lock);

    /* Add to hash table */
    hash = atom_hash_name(name, type);
    a->hash = hash;
    queue_enter(&as->hash_table[hash], a, atom_t, hash_chain);

    /* Add to type index */
    if (type < 128)
        queue_enter(&as->type_index[type], a, atom_t, type_chain);

    as->node_count++;

    simple_unlock(&as->lock);

    return a->handle;
}

/*
 * Add link
 */
atom_handle_t atomspace_add_link(atomspace_t as, atom_type_t type,
                                 atom_handle_t *outgoing, uint8_t arity)
{
    atom_t a;
    uint32_t hash;

    if (!as || !ATOM_IS_LINK(type) || arity > ATOM_MAX_OUTGOING)
        return ATOM_HANDLE_INVALID;

    if (arity > 0 && !outgoing)
        return ATOM_HANDLE_INVALID;

    simple_lock(&as->lock);

    /* Verify all outgoing atoms exist */
    for (int i = 0; i < arity; i++) {
        if (!atomspace_get_atom(as, outgoing[i])) {
            simple_unlock(&as->lock);
            return ATOM_HANDLE_INVALID;
        }
    }

    /* Allocate new atom */
    a = atomspace_alloc_atom(as);
    if (!a) {
        simple_unlock(&as->lock);
        return ATOM_HANDLE_INVALID;
    }

    /* Initialize atom */
    memset(a, 0, sizeof(struct atom));
    a->handle = as->next_handle++;
    a->type = type;
    a->arity = arity;
    a->flags = ATOM_FLAG_VALID;
    a->refcount = 1;

    /* Copy outgoing set */
    for (int i = 0; i < arity; i++)
        a->outgoing[i] = outgoing[i];

    /* Default truth value */
    a->tv.strength = TV_TRUE_STRENGTH;
    a->tv.confidence = TV_DEFAULT_CONF;

    /* Default attention */
    a->av.sti = AV_DEFAULT_STI;

    /* Initialize embedding */
    tensor_embedding_init(&a->embedding, a->handle, TENSOR_EMBED_DIM);

    simple_lock_init(&a->lock);

    /* Add to hash table */
    hash = atom_hash_link(outgoing, arity, type);
    a->hash = hash;
    queue_enter(&as->hash_table[hash], a, atom_t, hash_chain);

    /* Add to type index */
    if (type < 128)
        queue_enter(&as->type_index[type], a, atom_t, type_chain);

    as->link_count++;

    simple_unlock(&as->lock);

    return a->handle;
}

/*
 * Get atom by handle
 */
atom_t atomspace_get_atom(atomspace_t as, atom_handle_t handle)
{
    if (!as || handle == ATOM_HANDLE_INVALID)
        return NULL;

    /* Linear search (could optimize with handle->index mapping) */
    for (uint32_t i = 0; i < as->atom_count; i++) {
        if (as->atoms[i].handle == handle &&
            (as->atoms[i].flags & ATOM_FLAG_VALID))
            return &as->atoms[i];
    }

    return NULL;
}

/*
 * Remove atom
 */
kern_return_t atomspace_remove_atom(atomspace_t as, atom_handle_t handle)
{
    atom_t a;

    if (!as)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&as->lock);

    a = atomspace_get_atom(as, handle);
    if (!a) {
        simple_unlock(&as->lock);
        return KERN_INVALID_NAME;
    }

    /* Remove from hash table */
    queue_remove(&as->hash_table[a->hash], a, atom_t, hash_chain);

    /* Remove from type index */
    if (a->type < 128)
        queue_remove(&as->type_index[a->type], a, atom_t, type_chain);

    /* Mark as removed */
    a->flags = ATOM_FLAG_REMOVED;

    if (ATOM_IS_NODE(a->type))
        as->node_count--;
    else
        as->link_count--;

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Find node by name
 */
atom_handle_t atomspace_find_node(atomspace_t as, atom_type_t type,
                                  const char *name)
{
    uint32_t hash;
    atom_t a;

    if (!as || !name)
        return ATOM_HANDLE_INVALID;

    hash = atom_hash_name(name, type);

    queue_iterate(&as->hash_table[hash], a, atom_t, hash_chain) {
        if ((a->flags & ATOM_FLAG_VALID) &&
            a->type == type &&
            strcmp(a->name, name) == 0)
            return a->handle;
    }

    return ATOM_HANDLE_INVALID;
}

/*
 * Find by type
 */
kern_return_t atomspace_find_by_type(atomspace_t as, atom_type_t type,
                                     atom_query_result_t result)
{
    atom_t a;

    if (!as || !result || type >= 128)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    simple_lock(&as->lock);

    queue_iterate(&as->type_index[type], a, atom_t, type_chain) {
        if (a->flags & ATOM_FLAG_VALID) {
            result->total_matches++;
            if (result->count < 64)
                result->handles[result->count++] = a->handle;
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Find incoming links
 */
kern_return_t atomspace_find_incoming(atomspace_t as, atom_handle_t target,
                                      atom_query_result_t result)
{
    if (!as || !result)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        if (!(a->flags & ATOM_FLAG_VALID) || !ATOM_IS_LINK(a->type))
            continue;

        /* Check if target is in outgoing set */
        for (int j = 0; j < a->arity; j++) {
            if (a->outgoing[j] == target) {
                result->total_matches++;
                if (result->count < 64)
                    result->handles[result->count++] = a->handle;
                break;
            }
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Truth value operations
 */
void atom_set_truth_value(atom_t a, tensor_scalar_t strength,
                         tensor_scalar_t confidence)
{
    if (!a)
        return;

    simple_lock(&a->lock);
    a->tv.strength = strength;
    a->tv.confidence = confidence;
    simple_unlock(&a->lock);
}

void atom_get_truth_value(atom_t a, tensor_scalar_t *strength,
                         tensor_scalar_t *confidence)
{
    if (!a)
        return;

    simple_lock(&a->lock);
    if (strength) *strength = a->tv.strength;
    if (confidence) *confidence = a->tv.confidence;
    simple_unlock(&a->lock);
}

tensor_scalar_t atom_get_mean(atom_t a)
{
    if (!a)
        return 0;
    return a->tv.strength;
}

/*
 * Attention operations
 */
void atom_set_attention(atom_t a, int16_t sti, int16_t lti)
{
    if (!a)
        return;

    simple_lock(&a->lock);
    a->av.sti = sti;
    a->av.lti = lti;
    simple_unlock(&a->lock);
}

void atom_stimulate(atom_t a, int16_t amount)
{
    if (!a)
        return;

    simple_lock(&a->lock);
    a->av.sti += amount;
    simple_unlock(&a->lock);
}

void atom_decay_attention(atom_t a)
{
    if (!a)
        return;

    simple_lock(&a->lock);
    /* Decay STI towards zero */
    if (a->av.sti > 0)
        a->av.sti = a->av.sti * 9 / 10;
    else if (a->av.sti < 0)
        a->av.sti = a->av.sti * 9 / 10;
    simple_unlock(&a->lock);
}

/*
 * Embedding operations
 */
kern_return_t atom_set_embedding(atom_t a, tensor_embedding_t embed)
{
    if (!a || !embed)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    memcpy(&a->embedding, embed, sizeof(struct tensor_embedding));
    a->flags |= ATOM_FLAG_HAS_EMBED;
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

kern_return_t atom_get_embedding(atom_t a, tensor_embedding_t embed)
{
    if (!a || !embed)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    memcpy(embed, &a->embedding, sizeof(struct tensor_embedding));
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

/*
 * Compute embedding from structure
 */
kern_return_t atom_compute_embedding(atomspace_t as, atom_t a)
{
    struct tensor_embedding avg;

    if (!as || !a)
        return KERN_INVALID_ARGUMENT;

    /* For nodes, generate random embedding based on hash */
    if (ATOM_IS_NODE(a->type)) {
        tensor_embedding_random(&a->embedding, a->hash);
        a->flags |= ATOM_FLAG_HAS_EMBED;
        return KERN_SUCCESS;
    }

    /* For links, average outgoing embeddings */
    tensor_embedding_zero(&avg);
    avg.dim = TENSOR_EMBED_DIM;

    for (int i = 0; i < a->arity; i++) {
        atom_t target = atomspace_get_atom(as, a->outgoing[i]);
        if (target && (target->flags & ATOM_FLAG_HAS_EMBED)) {
            for (int j = 0; j < TENSOR_EMBED_DIM; j++)
                avg.values[j] += target->embedding.values[j];
        }
    }

    if (a->arity > 0) {
        tensor_scalar_t scale = TENSOR_FIXED_DIV(TENSOR_FIXED_POINT_ONE,
                                                TENSOR_TO_FIXED(a->arity));
        tensor_embedding_scale(&a->embedding, &avg, scale);
    }

    tensor_embedding_normalize(&a->embedding);
    a->flags |= ATOM_FLAG_HAS_EMBED;

    return KERN_SUCCESS;
}

/*
 * Semantic similarity
 */
tensor_scalar_t atomspace_similarity(atomspace_t as,
                                    atom_handle_t h1, atom_handle_t h2)
{
    atom_t a, b;

    if (!as)
        return 0;

    a = atomspace_get_atom(as, h1);
    b = atomspace_get_atom(as, h2);

    if (!a || !b)
        return 0;

    /* Ensure embeddings exist */
    if (!(a->flags & ATOM_FLAG_HAS_EMBED))
        atom_compute_embedding(as, a);
    if (!(b->flags & ATOM_FLAG_HAS_EMBED))
        atom_compute_embedding(as, b);

    return tensor_embedding_cosine_sim(&a->embedding, &b->embedding);
}

kern_return_t atomspace_find_similar(atomspace_t as, atom_handle_t target,
                                     tensor_scalar_t threshold,
                                     atom_query_result_t result)
{
    atom_t target_atom;

    if (!as || !result)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    target_atom = atomspace_get_atom(as, target);
    if (!target_atom)
        return KERN_INVALID_NAME;

    /* Ensure target has embedding */
    if (!(target_atom->flags & ATOM_FLAG_HAS_EMBED))
        atom_compute_embedding(as, target_atom);

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        tensor_scalar_t sim;

        if (!(a->flags & ATOM_FLAG_VALID) || a->handle == target)
            continue;

        if (!(a->flags & ATOM_FLAG_HAS_EMBED))
            atom_compute_embedding(as, a);

        sim = tensor_embedding_cosine_sim(&target_atom->embedding,
                                         &a->embedding);

        if (sim >= threshold) {
            result->total_matches++;
            if (result->count < 64)
                result->handles[result->count++] = a->handle;
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Kernel entity binding
 */
kern_return_t atom_bind_task(atom_t a, void *task)
{
    if (!a || a->type != ATOM_TYPE_TASK_NODE)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    a->kernel_object = task;
    a->flags |= ATOM_FLAG_KERNEL_BOUND;
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

kern_return_t atom_bind_thread(atom_t a, void *thread)
{
    if (!a || a->type != ATOM_TYPE_THREAD_NODE)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    a->kernel_object = thread;
    a->flags |= ATOM_FLAG_KERNEL_BOUND;
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

kern_return_t atom_bind_port(atom_t a, void *port)
{
    if (!a || a->type != ATOM_TYPE_PORT_NODE)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    a->kernel_object = port;
    a->flags |= ATOM_FLAG_KERNEL_BOUND;
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

kern_return_t atom_bind_processor(atom_t a, void *processor)
{
    if (!a || a->type != ATOM_TYPE_PROCESSOR_NODE)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&a->lock);
    a->kernel_object = processor;
    a->flags |= ATOM_FLAG_KERNEL_BOUND;
    simple_unlock(&a->lock);

    return KERN_SUCCESS;
}

/*
 * Pattern matching
 */
kern_return_t atomspace_filter(atomspace_t as, atom_predicate_t pred,
                              void *context, atom_query_result_t result)
{
    if (!as || !pred || !result)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        if (!(a->flags & ATOM_FLAG_VALID))
            continue;

        if (pred(a, context)) {
            result->total_matches++;
            if (result->count < 64)
                result->handles[result->count++] = a->handle;
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Inheritance queries
 */
kern_return_t atomspace_get_inheritance(atomspace_t as, atom_handle_t child,
                                        atom_query_result_t result)
{
    if (!as || !result)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        if (!(a->flags & ATOM_FLAG_VALID) ||
            a->type != ATOM_TYPE_INHERITANCE_LINK)
            continue;

        /* InheritanceLink(child, parent) */
        if (a->arity >= 2 && a->outgoing[0] == child) {
            result->total_matches++;
            if (result->count < 64)
                result->handles[result->count++] = a->outgoing[1];
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

boolean_t atomspace_inherits_from(atomspace_t as, atom_handle_t child,
                                  atom_handle_t parent)
{
    struct atom_query_result result;

    if (atomspace_get_inheritance(as, child, &result) != KERN_SUCCESS)
        return FALSE;

    for (uint32_t i = 0; i < result.count; i++) {
        if (result.handles[i] == parent)
            return TRUE;
        /* Recursive check */
        if (atomspace_inherits_from(as, result.handles[i], parent))
            return TRUE;
    }

    return FALSE;
}

/*
 * ECAN attention allocation
 */
void atomspace_spread_attention(atomspace_t as)
{
    if (!as)
        return;

    simple_lock(&as->lock);

    /* Spread attention through links */
    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        if (!(a->flags & ATOM_FLAG_VALID) || !ATOM_IS_LINK(a->type))
            continue;

        if (a->av.sti > 10) {
            int16_t spread = a->av.sti / (a->arity + 1);
            for (int j = 0; j < a->arity; j++) {
                atom_t target = atomspace_get_atom(as, a->outgoing[j]);
                if (target)
                    atom_stimulate(target, spread);
            }
            a->av.sti -= spread * a->arity;
        }
    }

    simple_unlock(&as->lock);
}

void atomspace_decay_attention(atomspace_t as)
{
    if (!as)
        return;

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count; i++) {
        atom_t a = &as->atoms[i];
        if (a->flags & ATOM_FLAG_VALID)
            atom_decay_attention(a);
    }

    simple_unlock(&as->lock);
}

kern_return_t atomspace_get_attentional_focus(atomspace_t as, uint32_t count,
                                              atom_query_result_t result)
{
    int16_t threshold = AV_IMPORTANT_STI;

    if (!as || !result)
        return KERN_INVALID_ARGUMENT;

    result->count = 0;
    result->total_matches = 0;

    simple_lock(&as->lock);

    for (uint32_t i = 0; i < as->atom_count && result->count < count; i++) {
        atom_t a = &as->atoms[i];
        if ((a->flags & ATOM_FLAG_VALID) && a->av.sti >= threshold) {
            result->total_matches++;
            if (result->count < 64)
                result->handles[result->count++] = a->handle;
        }
    }

    simple_unlock(&as->lock);

    return KERN_SUCCESS;
}

/*
 * Statistics
 */
uint32_t atomspace_get_size(atomspace_t as)
{
    return as ? as->node_count + as->link_count : 0;
}

uint32_t atomspace_get_node_count(atomspace_t as)
{
    return as ? as->node_count : 0;
}

uint32_t atomspace_get_link_count(atomspace_t as)
{
    return as ? as->link_count : 0;
}

void atomspace_print_stats(atomspace_t as)
{
    if (!as)
        return;

    printf("atomspace: nodes=%u links=%u total=%u queries=%u\n",
           as->node_count, as->link_count,
           as->node_count + as->link_count,
           as->query_count);
}
