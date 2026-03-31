/*
 * GNU Mach Tensor Logic Subsystem Implementation
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include <kern/tensor_logic.h>
#include <kern/kalloc.h>
#include <kern/printf.h>
#include <string.h>

/*
 * Global default Tensor Logic context
 */
static struct tensor_logic_context tl_default_context;
static boolean_t tl_initialized = FALSE;

/*
 * Category to atom type mapping
 */
static const atom_type_t category_atom_types[] = {
    [TL_ENTITY_TASK]      = ATOM_TYPE_TASK_NODE,
    [TL_ENTITY_THREAD]    = ATOM_TYPE_THREAD_NODE,
    [TL_ENTITY_PORT]      = ATOM_TYPE_PORT_NODE,
    [TL_ENTITY_PROCESSOR] = ATOM_TYPE_PROCESSOR_NODE,
    [TL_ENTITY_MEMORY]    = ATOM_TYPE_MEMORY_NODE,
    [TL_ENTITY_DEVICE]    = ATOM_TYPE_DEVICE_NODE,
    [TL_ENTITY_NETWORK]   = ATOM_TYPE_NETWORK_NODE,
};

/*
 * Category names for debugging
 */
static const char *category_names[] = {
    [TL_ENTITY_TASK]      = "task",
    [TL_ENTITY_THREAD]    = "thread",
    [TL_ENTITY_PORT]      = "port",
    [TL_ENTITY_PROCESSOR] = "processor",
    [TL_ENTITY_MEMORY]    = "memory",
    [TL_ENTITY_DEVICE]    = "device",
    [TL_ENTITY_NETWORK]   = "network",
};

/*
 * Relationship to link type mapping
 */
static const atom_type_t rel_link_types[] = {
    [TL_REL_CONTAINS]     = ATOM_TYPE_MEMBER_LINK,
    [TL_REL_COMMUNICATES] = ATOM_TYPE_IPC_LINK,
    [TL_REL_DEPENDS]      = ATOM_TYPE_DEPENDS_LINK,
    [TL_REL_COMPETES]     = ATOM_TYPE_EXECUTION_LINK,
    [TL_REL_AFFINITY]     = ATOM_TYPE_AFFINITY_LINK,
    [TL_REL_SIMILAR]      = ATOM_TYPE_SIMILARITY_LINK,
    [TL_REL_TEMPORAL]     = ATOM_TYPE_TEMPORAL_LINK,
};

/*
 * Forward declarations
 */
static tl_entity_t tl_find_entity(tensor_logic_context_t ctx, atom_handle_t h);
static uint32_t tl_allocate_entity(tensor_logic_context_t ctx);
static void tl_init_category_roots(tensor_logic_context_t ctx);

/*
 * ============================================================
 * TENSOR LOGIC LIFECYCLE
 * ============================================================
 */

void
tensor_logic_init(void)
{
    if (tl_initialized)
        return;

    /* Initialize subsystems */
    tensor_subsystem_init();
    atomspace_subsystem_init();

    /* Initialize default context */
    memset(&tl_default_context, 0, sizeof(tl_default_context));
    simple_lock_init(&tl_default_context.lock);

    /* Create atomspace */
    atomspace_create(&tl_default_context.atomspace);

    /* Initialize category roots */
    tl_init_category_roots(&tl_default_context);

    /* Set defaults */
    tl_default_context.similarity_threshold = TL_SIMILARITY_THRESHOLD;
    tl_default_context.attention_threshold = TL_ATTENTION_THRESHOLD;
    tl_default_context.flags = TL_FLAG_INITIALIZED | TL_FLAG_AUTO_EMBED;

    tl_initialized = TRUE;

    printf("tensor_logic: initialized (max_entities=%d)\n", TL_MAX_ENTITIES);
}

kern_return_t
tensor_logic_create(tensor_logic_context_t *ctx)
{
    tensor_logic_context_t new_ctx;

    if (ctx == NULL)
        return KERN_INVALID_ARGUMENT;

    new_ctx = (tensor_logic_context_t)kalloc(sizeof(struct tensor_logic_context));
    if (new_ctx == NULL)
        return KERN_RESOURCE_SHORTAGE;

    memset(new_ctx, 0, sizeof(struct tensor_logic_context));
    simple_lock_init(&new_ctx->lock);

    if (atomspace_create(&new_ctx->atomspace) != KERN_SUCCESS) {
        kfree((vm_offset_t)new_ctx, sizeof(struct tensor_logic_context));
        return KERN_RESOURCE_SHORTAGE;
    }

    tl_init_category_roots(new_ctx);

    new_ctx->similarity_threshold = TL_SIMILARITY_THRESHOLD;
    new_ctx->attention_threshold = TL_ATTENTION_THRESHOLD;
    new_ctx->flags = TL_FLAG_INITIALIZED | TL_FLAG_AUTO_EMBED;

    *ctx = new_ctx;
    return KERN_SUCCESS;
}

void
tensor_logic_destroy(tensor_logic_context_t ctx)
{
    if (ctx == NULL || ctx == &tl_default_context)
        return;

    atomspace_destroy(ctx->atomspace);
    kfree((vm_offset_t)ctx, sizeof(struct tensor_logic_context));
}

tensor_logic_context_t
tensor_logic_get_default(void)
{
    if (!tl_initialized)
        tensor_logic_init();
    return &tl_default_context;
}

/*
 * Initialize category root nodes
 */
static void
tl_init_category_roots(tensor_logic_context_t ctx)
{
    int i;
    char name[32];

    for (i = 0; i < TL_ENTITY_COUNT; i++) {
        snprintf(name, sizeof(name), "category:%s", category_names[i]);
        ctx->category_roots[i] = atomspace_add_node(
            ctx->atomspace, ATOM_TYPE_CONCEPT_NODE, name);
    }
}

/*
 * Find entity by atom handle
 */
static tl_entity_t
tl_find_entity(tensor_logic_context_t ctx, atom_handle_t h)
{
    uint32_t i;

    for (i = 0; i < ctx->entity_count; i++) {
        if (ctx->entities[i].atom == h)
            return &ctx->entities[i];
    }
    return NULL;
}

/*
 * Allocate new entity slot
 */
static uint32_t
tl_allocate_entity(tensor_logic_context_t ctx)
{
    if (ctx->entity_count >= TL_MAX_ENTITIES)
        return (uint32_t)-1;

    return ctx->entity_count++;
}

/*
 * ============================================================
 * MULTI-ENTITY TRACKING
 * ============================================================
 */

kern_return_t
tl_register_task(tensor_logic_context_t ctx, void *task,
                 const char *name, atom_handle_t *handle)
{
    atom_handle_t h;
    uint32_t idx;
    tl_entity_t entity;
    atom_handle_t outgoing[2];
    atom_t atom;

    if (ctx == NULL || task == NULL || name == NULL || handle == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    /* Create node in atomspace */
    h = atomspace_add_node(ctx->atomspace, ATOM_TYPE_TASK_NODE, name);
    if (h == ATOM_HANDLE_INVALID) {
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    /* Allocate entity slot */
    idx = tl_allocate_entity(ctx);
    if (idx == (uint32_t)-1) {
        atomspace_remove_atom(ctx->atomspace, h);
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    /* Initialize entity */
    entity = &ctx->entities[idx];
    entity->atom = h;
    entity->category = TL_ENTITY_TASK;
    entity->scale = TL_SCALE_TASK;
    entity->kernel_id = ctx->next_entity_id++;
    entity->kernel_ptr = task;
    entity->flags = TL_ENTITY_FLAG_ACTIVE;

    /* Initialize embedding */
    tensor_embedding_init(&entity->embedding, entity->kernel_id, TENSOR_EMBED_DIM);
    if (ctx->flags & TL_FLAG_AUTO_EMBED) {
        tensor_embedding_random(&entity->embedding, (uint32_t)(uintptr_t)task);
        tensor_embedding_normalize(&entity->embedding);
    }

    /* Bind kernel object */
    atom = atomspace_get_atom(ctx->atomspace, h);
    if (atom != NULL) {
        atom_bind_task(atom, task);
        atom_set_embedding(atom, &entity->embedding);
    }

    /* Create inheritance link to category */
    outgoing[0] = h;
    outgoing[1] = ctx->category_roots[TL_ENTITY_TASK];
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_INHERITANCE_LINK, outgoing, 2);

    ctx->updates++;
    *handle = h;

    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_register_thread(tensor_logic_context_t ctx, void *thread,
                   atom_handle_t task, const char *name,
                   atom_handle_t *handle)
{
    atom_handle_t h;
    uint32_t idx;
    tl_entity_t entity;
    atom_handle_t outgoing[2];
    atom_t atom;

    if (ctx == NULL || thread == NULL || name == NULL || handle == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    h = atomspace_add_node(ctx->atomspace, ATOM_TYPE_THREAD_NODE, name);
    if (h == ATOM_HANDLE_INVALID) {
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    idx = tl_allocate_entity(ctx);
    if (idx == (uint32_t)-1) {
        atomspace_remove_atom(ctx->atomspace, h);
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    entity = &ctx->entities[idx];
    entity->atom = h;
    entity->category = TL_ENTITY_THREAD;
    entity->scale = TL_SCALE_THREAD;
    entity->kernel_id = ctx->next_entity_id++;
    entity->kernel_ptr = thread;
    entity->flags = TL_ENTITY_FLAG_ACTIVE;

    tensor_embedding_init(&entity->embedding, entity->kernel_id, TENSOR_EMBED_DIM);
    if (ctx->flags & TL_FLAG_AUTO_EMBED) {
        tensor_embedding_random(&entity->embedding, (uint32_t)(uintptr_t)thread);
        tensor_embedding_normalize(&entity->embedding);
    }

    atom = atomspace_get_atom(ctx->atomspace, h);
    if (atom != NULL) {
        atom_bind_thread(atom, thread);
        atom_set_embedding(atom, &entity->embedding);
    }

    /* Link to category */
    outgoing[0] = h;
    outgoing[1] = ctx->category_roots[TL_ENTITY_THREAD];
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_INHERITANCE_LINK, outgoing, 2);

    /* Link thread to task (membership) */
    if (task != ATOM_HANDLE_INVALID) {
        outgoing[0] = h;
        outgoing[1] = task;
        atomspace_add_link(ctx->atomspace, ATOM_TYPE_MEMBER_LINK, outgoing, 2);
    }

    ctx->updates++;
    *handle = h;

    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_register_port(tensor_logic_context_t ctx, void *port,
                 atom_handle_t owner, const char *name,
                 atom_handle_t *handle)
{
    atom_handle_t h;
    uint32_t idx;
    tl_entity_t entity;
    atom_handle_t outgoing[2];
    atom_t atom;

    if (ctx == NULL || port == NULL || name == NULL || handle == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    h = atomspace_add_node(ctx->atomspace, ATOM_TYPE_PORT_NODE, name);
    if (h == ATOM_HANDLE_INVALID) {
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    idx = tl_allocate_entity(ctx);
    if (idx == (uint32_t)-1) {
        atomspace_remove_atom(ctx->atomspace, h);
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    entity = &ctx->entities[idx];
    entity->atom = h;
    entity->category = TL_ENTITY_PORT;
    entity->scale = TL_SCALE_TASK;  /* Ports are task-level */
    entity->kernel_id = ctx->next_entity_id++;
    entity->kernel_ptr = port;
    entity->flags = TL_ENTITY_FLAG_ACTIVE;

    tensor_embedding_init(&entity->embedding, entity->kernel_id, TENSOR_EMBED_DIM);
    if (ctx->flags & TL_FLAG_AUTO_EMBED) {
        tensor_embedding_random(&entity->embedding, (uint32_t)(uintptr_t)port);
        tensor_embedding_normalize(&entity->embedding);
    }

    atom = atomspace_get_atom(ctx->atomspace, h);
    if (atom != NULL) {
        atom_bind_port(atom, port);
        atom_set_embedding(atom, &entity->embedding);
    }

    /* Link to category */
    outgoing[0] = h;
    outgoing[1] = ctx->category_roots[TL_ENTITY_PORT];
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_INHERITANCE_LINK, outgoing, 2);

    /* Link port to owner */
    if (owner != ATOM_HANDLE_INVALID) {
        outgoing[0] = h;
        outgoing[1] = owner;
        atomspace_add_link(ctx->atomspace, ATOM_TYPE_MEMBER_LINK, outgoing, 2);
    }

    ctx->updates++;
    *handle = h;

    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_register_processor(tensor_logic_context_t ctx, void *processor,
                      int cpu_id, atom_handle_t *handle)
{
    atom_handle_t h;
    uint32_t idx;
    tl_entity_t entity;
    atom_handle_t outgoing[2];
    atom_t atom;
    char name[32];

    if (ctx == NULL || processor == NULL || handle == NULL)
        return KERN_INVALID_ARGUMENT;

    snprintf(name, sizeof(name), "cpu%d", cpu_id);

    simple_lock(&ctx->lock);

    h = atomspace_add_node(ctx->atomspace, ATOM_TYPE_PROCESSOR_NODE, name);
    if (h == ATOM_HANDLE_INVALID) {
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    idx = tl_allocate_entity(ctx);
    if (idx == (uint32_t)-1) {
        atomspace_remove_atom(ctx->atomspace, h);
        simple_unlock(&ctx->lock);
        return KERN_RESOURCE_SHORTAGE;
    }

    entity = &ctx->entities[idx];
    entity->atom = h;
    entity->category = TL_ENTITY_PROCESSOR;
    entity->scale = TL_SCALE_HOST;
    entity->kernel_id = cpu_id;
    entity->kernel_ptr = processor;
    entity->flags = TL_ENTITY_FLAG_ACTIVE;

    tensor_embedding_init(&entity->embedding, entity->kernel_id, TENSOR_EMBED_DIM);
    if (ctx->flags & TL_FLAG_AUTO_EMBED) {
        tensor_embedding_random(&entity->embedding, cpu_id);
        tensor_embedding_normalize(&entity->embedding);
    }

    atom = atomspace_get_atom(ctx->atomspace, h);
    if (atom != NULL) {
        atom_bind_processor(atom, processor);
        atom_set_embedding(atom, &entity->embedding);
    }

    /* Link to category */
    outgoing[0] = h;
    outgoing[1] = ctx->category_roots[TL_ENTITY_PROCESSOR];
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_INHERITANCE_LINK, outgoing, 2);

    ctx->updates++;
    *handle = h;

    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_unregister_entity(tensor_logic_context_t ctx, atom_handle_t handle)
{
    tl_entity_t entity;

    if (ctx == NULL || handle == ATOM_HANDLE_INVALID)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    entity = tl_find_entity(ctx, handle);
    if (entity == NULL) {
        simple_unlock(&ctx->lock);
        return KERN_INVALID_NAME;
    }

    entity->flags &= ~TL_ENTITY_FLAG_ACTIVE;
    atomspace_remove_atom(ctx->atomspace, handle);

    ctx->updates++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_update_embedding(tensor_logic_context_t ctx, atom_handle_t handle,
                    tensor_embedding_t new_embed)
{
    tl_entity_t entity;
    atom_t atom;

    if (ctx == NULL || handle == ATOM_HANDLE_INVALID || new_embed == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    entity = tl_find_entity(ctx, handle);
    if (entity == NULL) {
        simple_unlock(&ctx->lock);
        return KERN_INVALID_NAME;
    }

    /* Update local copy */
    memcpy(&entity->embedding, new_embed, sizeof(struct tensor_embedding));

    /* Update atom's embedding */
    atom = atomspace_get_atom(ctx->atomspace, handle);
    if (atom != NULL) {
        atom_set_embedding(atom, new_embed);
    }

    ctx->updates++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

void
tl_stimulate_entity(tensor_logic_context_t ctx, atom_handle_t handle,
                    int16_t amount)
{
    atom_t atom;

    if (ctx == NULL || handle == ATOM_HANDLE_INVALID)
        return;

    simple_lock(&ctx->lock);

    atom = atomspace_get_atom(ctx->atomspace, handle);
    if (atom != NULL) {
        atom_stimulate(atom, amount);
    }

    simple_unlock(&ctx->lock);
}

/*
 * ============================================================
 * NETWORK-AWARE OPERATIONS
 * ============================================================
 */

kern_return_t
tl_record_ipc(tensor_logic_context_t ctx, atom_handle_t sender,
              atom_handle_t receiver, uint32_t message_size)
{
    atom_handle_t outgoing[2];
    atom_handle_t link;
    atom_t link_atom;
    tensor_scalar_t strength;

    if (ctx == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    /* Create IPC link */
    outgoing[0] = sender;
    outgoing[1] = receiver;
    link = atomspace_add_link(ctx->atomspace, ATOM_TYPE_IPC_LINK, outgoing, 2);

    if (link != ATOM_HANDLE_INVALID) {
        /* Set truth value based on message size */
        link_atom = atomspace_get_atom(ctx->atomspace, link);
        if (link_atom != NULL) {
            /* Normalize message size to [0, 1] range */
            strength = (message_size > 65536) ?
                       TENSOR_FIXED_POINT_ONE :
                       TENSOR_FIXED_MUL(TENSOR_TO_FIXED(message_size),
                                       TENSOR_FIXED_DIV(TENSOR_FIXED_POINT_ONE, TENSOR_TO_FIXED(65536)));
            atom_set_truth_value(link_atom, strength, TV_DEFAULT_CONF);

            /* Stimulate both sender and receiver */
            tl_stimulate_entity(ctx, sender, 5);
            tl_stimulate_entity(ctx, receiver, 5);
        }
    }

    ctx->updates++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_record_dependency(tensor_logic_context_t ctx, atom_handle_t dependent,
                     atom_handle_t resource)
{
    atom_handle_t outgoing[2];

    if (ctx == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    outgoing[0] = dependent;
    outgoing[1] = resource;
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_DEPENDS_LINK, outgoing, 2);

    ctx->updates++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_record_affinity(tensor_logic_context_t ctx, atom_handle_t thread,
                   atom_handle_t processor)
{
    atom_handle_t outgoing[2];

    if (ctx == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    outgoing[0] = thread;
    outgoing[1] = processor;
    atomspace_add_link(ctx->atomspace, ATOM_TYPE_AFFINITY_LINK, outgoing, 2);

    ctx->updates++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_analyze_network(tensor_logic_context_t ctx, tl_entity_category_t category,
                   struct tl_network_analysis *result)
{
    struct atom_query_result entities;
    struct atom_query_result links;
    uint32_t i, j;
    uint32_t max_degree = 0;
    atom_t atom;
    uint32_t degree_sum = 0;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    memset(result, 0, sizeof(*result));

    /* Get all entities of this category */
    atomspace_find_by_type(ctx->atomspace, category_atom_types[category], &entities);
    result->node_count = entities.count;

    /* Get all IPC links */
    atomspace_find_by_type(ctx->atomspace, ATOM_TYPE_IPC_LINK, &links);
    result->edge_count = links.count;

    /* Calculate density */
    if (result->node_count > 1) {
        uint32_t max_edges = (result->node_count * (result->node_count - 1)) / 2;
        if (max_edges > 0) {
            result->density = TENSOR_FIXED_DIV(
                TENSOR_TO_FIXED(result->edge_count),
                TENSOR_TO_FIXED(max_edges));
        }
    }

    /* Find hubs (high-degree nodes) */
    result->hub_count = 0;
    for (i = 0; i < entities.count && i < 64; i++) {
        struct atom_query_result incoming;
        atomspace_find_incoming(ctx->atomspace, entities.handles[i], &incoming);

        degree_sum += incoming.count;

        if (incoming.count > max_degree) {
            max_degree = incoming.count;
        }

        /* Consider as hub if degree > 3 */
        if (incoming.count > 3 && result->hub_count < 8) {
            result->hubs[result->hub_count++] = entities.handles[i];
        }
    }

    /* Detect topology based on patterns */
    if (result->hub_count == 1 && result->edge_count > result->node_count / 2) {
        result->topology = TL_TOPO_STAR;
    } else if (result->density > TENSOR_FIXED_POINT_ONE / 2) {
        result->topology = TL_TOPO_MESH;
    } else if (result->edge_count == result->node_count) {
        result->topology = TL_TOPO_RING;
    } else if (result->hub_count > 0) {
        result->topology = TL_TOPO_TREE;
    } else {
        result->topology = TL_TOPO_PIPELINE;
    }

    /* Simple clustering coefficient estimate */
    if (result->node_count > 2 && degree_sum > 0) {
        result->clustering = TENSOR_FIXED_DIV(
            TENSOR_TO_FIXED(result->edge_count * 2),
            TENSOR_TO_FIXED(degree_sum));
    }

    /* Assume single component for now */
    result->component_count = 1;

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_find_hubs(tensor_logic_context_t ctx, tl_entity_category_t category,
             atom_query_result_t result)
{
    struct tl_network_analysis analysis;
    uint32_t i;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    tl_analyze_network(ctx, category, &analysis);

    result->count = 0;
    for (i = 0; i < analysis.hub_count && i < 64; i++) {
        result->handles[result->count++] = analysis.hubs[i];
    }
    result->total_matches = analysis.hub_count;

    return KERN_SUCCESS;
}

/*
 * ============================================================
 * MULTI-SCALE ANALYSIS
 * ============================================================
 */

kern_return_t
tl_aggregate_scale(tensor_logic_context_t ctx, tl_scale_t scale,
                   struct tl_scale_aggregate *result)
{
    uint32_t i;
    uint32_t count = 0;
    int32_t total_sti = 0;
    tensor_scalar_t max_sti = 0;
    atom_handle_t best_rep = ATOM_HANDLE_INVALID;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    memset(result, 0, sizeof(*result));
    result->scale = scale;
    tensor_embedding_zero(&result->mean_embedding);

    /* Aggregate entities at this scale */
    for (i = 0; i < ctx->entity_count; i++) {
        tl_entity_t entity = &ctx->entities[i];

        if (!(entity->flags & TL_ENTITY_FLAG_ACTIVE))
            continue;

        if (entity->scale == scale) {
            atom_t atom = atomspace_get_atom(ctx->atomspace, entity->atom);

            /* Accumulate embedding */
            tensor_embedding_add(&result->mean_embedding,
                                &result->mean_embedding,
                                &entity->embedding);

            /* Track attention */
            if (atom != NULL) {
                total_sti += atom->av.sti;
                if (atom->av.sti > max_sti) {
                    max_sti = atom->av.sti;
                    best_rep = entity->atom;
                }
            }

            count++;
        }
    }

    if (count > 0) {
        /* Average the embedding */
        tensor_scalar_t inv_count = TENSOR_FIXED_DIV(
            TENSOR_FIXED_POINT_ONE, TENSOR_TO_FIXED(count));
        tensor_embedding_scale(&result->mean_embedding,
                              &result->mean_embedding, inv_count);
    }

    result->entity_count = count;
    result->total_attention = total_sti;
    result->representative = best_rep;

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

kern_return_t
tl_get_scale_entities(tensor_logic_context_t ctx, tl_scale_t scale,
                      atom_query_result_t result)
{
    uint32_t i;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    result->count = 0;
    result->total_matches = 0;

    for (i = 0; i < ctx->entity_count; i++) {
        tl_entity_t entity = &ctx->entities[i];

        if (!(entity->flags & TL_ENTITY_FLAG_ACTIVE))
            continue;

        if (entity->scale == scale) {
            result->total_matches++;
            if (result->count < 64) {
                result->handles[result->count++] = entity->atom;
            }
        }
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

/*
 * ============================================================
 * TENSOR-ENHANCED SEMANTICS
 * ============================================================
 */

kern_return_t
tl_find_similar(tensor_logic_context_t ctx, atom_handle_t target,
                tensor_scalar_t threshold, atom_query_result_t result)
{
    tl_entity_t target_entity;
    uint32_t i;
    tensor_scalar_t sim;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    target_entity = tl_find_entity(ctx, target);
    if (target_entity == NULL) {
        simple_unlock(&ctx->lock);
        return KERN_INVALID_NAME;
    }

    result->count = 0;
    result->total_matches = 0;

    for (i = 0; i < ctx->entity_count; i++) {
        tl_entity_t entity = &ctx->entities[i];

        if (!(entity->flags & TL_ENTITY_FLAG_ACTIVE))
            continue;

        if (entity->atom == target)
            continue;

        /* Compare embeddings */
        sim = tensor_embedding_cosine_sim(&target_entity->embedding,
                                         &entity->embedding);

        if (sim >= threshold) {
            result->total_matches++;
            if (result->count < 64) {
                result->handles[result->count++] = entity->atom;
            }
        }
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

tensor_scalar_t
tl_similarity(tensor_logic_context_t ctx, atom_handle_t a, atom_handle_t b)
{
    tl_entity_t ea, eb;
    tensor_scalar_t sim = 0;

    if (ctx == NULL)
        return 0;

    simple_lock(&ctx->lock);

    ea = tl_find_entity(ctx, a);
    eb = tl_find_entity(ctx, b);

    if (ea != NULL && eb != NULL) {
        sim = tensor_embedding_cosine_sim(&ea->embedding, &eb->embedding);
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return sim;
}

kern_return_t
tl_cluster_entities(tensor_logic_context_t ctx, tl_entity_category_t category)
{
    /* Simple single-pass clustering using greedy assignment */
    uint32_t i, j;
    tensor_scalar_t threshold = ctx->similarity_threshold;

    if (ctx == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    /* Reset clusters */
    ctx->cluster_count = 0;

    for (i = 0; i < ctx->entity_count; i++) {
        tl_entity_t entity = &ctx->entities[i];
        boolean_t assigned = FALSE;

        if (!(entity->flags & TL_ENTITY_FLAG_ACTIVE))
            continue;

        if (entity->category != category)
            continue;

        /* Try to assign to existing cluster */
        for (j = 0; j < ctx->cluster_count; j++) {
            tl_cluster_t cluster = &ctx->clusters[j];
            tensor_scalar_t sim;

            sim = tensor_embedding_cosine_sim(&entity->embedding,
                                             &cluster->centroid);

            if (sim >= threshold && cluster->count < 64) {
                cluster->atoms[cluster->count++] = entity->atom;
                entity->flags |= TL_ENTITY_FLAG_CLUSTERED;
                assigned = TRUE;
                break;
            }
        }

        /* Create new cluster if not assigned */
        if (!assigned && ctx->cluster_count < 32) {
            tl_cluster_t cluster = &ctx->clusters[ctx->cluster_count];

            memset(cluster, 0, sizeof(*cluster));
            cluster->cluster_id = ctx->cluster_count;
            cluster->category = category;
            cluster->atoms[0] = entity->atom;
            cluster->count = 1;
            memcpy(&cluster->centroid, &entity->embedding,
                   sizeof(struct tensor_embedding));

            entity->flags |= TL_ENTITY_FLAG_CLUSTERED;
            ctx->cluster_count++;
        }
    }

    /* Recompute centroids */
    for (j = 0; j < ctx->cluster_count; j++) {
        tl_compute_centroid(ctx, &ctx->clusters[j]);
    }

    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

tl_cluster_t
tl_get_cluster(tensor_logic_context_t ctx, atom_handle_t entity)
{
    uint32_t i, j;

    if (ctx == NULL)
        return NULL;

    for (i = 0; i < ctx->cluster_count; i++) {
        tl_cluster_t cluster = &ctx->clusters[i];
        for (j = 0; j < cluster->count; j++) {
            if (cluster->atoms[j] == entity)
                return cluster;
        }
    }

    return NULL;
}

kern_return_t
tl_compute_centroid(tensor_logic_context_t ctx, tl_cluster_t cluster)
{
    uint32_t i;
    tl_entity_t entity;

    if (ctx == NULL || cluster == NULL)
        return KERN_INVALID_ARGUMENT;

    tensor_embedding_zero(&cluster->centroid);

    for (i = 0; i < cluster->count; i++) {
        entity = tl_find_entity(ctx, cluster->atoms[i]);
        if (entity != NULL) {
            tensor_embedding_add(&cluster->centroid,
                                &cluster->centroid,
                                &entity->embedding);
        }
    }

    if (cluster->count > 0) {
        tensor_scalar_t inv = TENSOR_FIXED_DIV(
            TENSOR_FIXED_POINT_ONE, TENSOR_TO_FIXED(cluster->count));
        tensor_embedding_scale(&cluster->centroid,
                              &cluster->centroid, inv);
        tensor_embedding_normalize(&cluster->centroid);
    }

    return KERN_SUCCESS;
}

/*
 * ============================================================
 * ATTENTION AND FOCUS
 * ============================================================
 */

kern_return_t
tl_get_focus(tensor_logic_context_t ctx, uint32_t count,
             atom_query_result_t result)
{
    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);
    atomspace_get_attentional_focus(ctx->atomspace, count, result);
    ctx->queries++;
    simple_unlock(&ctx->lock);

    return KERN_SUCCESS;
}

void
tl_spread_attention(tensor_logic_context_t ctx)
{
    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);
    atomspace_spread_attention(ctx->atomspace);
    simple_unlock(&ctx->lock);
}

void
tl_decay_attention(tensor_logic_context_t ctx)
{
    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);
    atomspace_decay_attention(ctx->atomspace);
    simple_unlock(&ctx->lock);
}

/*
 * ============================================================
 * INFERENCE AND QUERIES
 * ============================================================
 */

kern_return_t
tl_query(tensor_logic_context_t ctx, tl_entity_category_t category,
         atom_predicate_t predicate, void *pred_context,
         atom_query_result_t result)
{
    struct atom_query_result type_results;
    uint32_t i;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    /* Get all of this type */
    atomspace_find_by_type(ctx->atomspace, category_atom_types[category],
                          &type_results);

    result->count = 0;
    result->total_matches = 0;

    /* Filter by predicate if provided */
    for (i = 0; i < type_results.count; i++) {
        atom_t atom = atomspace_get_atom(ctx->atomspace, type_results.handles[i]);

        if (atom == NULL)
            continue;

        if (predicate == NULL || predicate(atom, pred_context)) {
            result->total_matches++;
            if (result->count < 64) {
                result->handles[result->count++] = type_results.handles[i];
            }
        }
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

boolean_t
tl_is_a(tensor_logic_context_t ctx, atom_handle_t entity, atom_handle_t type)
{
    boolean_t result;

    if (ctx == NULL)
        return FALSE;

    simple_lock(&ctx->lock);
    result = atomspace_inherits_from(ctx->atomspace, entity, type);
    ctx->inferences++;
    simple_unlock(&ctx->lock);

    return result;
}

kern_return_t
tl_get_relationships(tensor_logic_context_t ctx, atom_handle_t entity,
                     tl_relationship_t rel_type, atom_query_result_t result)
{
    struct atom_query_result links;
    uint32_t i;
    atom_type_t link_type;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    simple_lock(&ctx->lock);

    link_type = rel_link_types[rel_type];
    atomspace_find_by_type(ctx->atomspace, link_type, &links);

    result->count = 0;
    result->total_matches = 0;

    /* Find links involving this entity */
    for (i = 0; i < links.count; i++) {
        atom_t link = atomspace_get_atom(ctx->atomspace, links.handles[i]);
        uint32_t j;

        if (link == NULL)
            continue;

        for (j = 0; j < link->arity; j++) {
            if (link->outgoing[j] == entity) {
                result->total_matches++;
                if (result->count < 64) {
                    result->handles[result->count++] = links.handles[i];
                }
                break;
            }
        }
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

/*
 * ============================================================
 * ANOMALY DETECTION
 * ============================================================
 */

kern_return_t
tl_detect_anomalies(tensor_logic_context_t ctx, tl_entity_category_t category,
                    tensor_scalar_t threshold, atom_query_result_t result)
{
    struct tl_scale_aggregate agg;
    uint32_t i;
    tensor_scalar_t score;

    if (ctx == NULL || result == NULL)
        return KERN_INVALID_ARGUMENT;

    /* First compute the mean embedding for this category */
    /* Map category to scale for aggregation */
    tl_scale_t scale;
    switch (category) {
    case TL_ENTITY_THREAD:
        scale = TL_SCALE_THREAD;
        break;
    case TL_ENTITY_TASK:
    case TL_ENTITY_PORT:
        scale = TL_SCALE_TASK;
        break;
    default:
        scale = TL_SCALE_HOST;
        break;
    }

    tl_aggregate_scale(ctx, scale, &agg);

    simple_lock(&ctx->lock);

    result->count = 0;
    result->total_matches = 0;

    for (i = 0; i < ctx->entity_count; i++) {
        tl_entity_t entity = &ctx->entities[i];

        if (!(entity->flags & TL_ENTITY_FLAG_ACTIVE))
            continue;

        if (entity->category != category)
            continue;

        /* Compute distance from mean */
        score = tensor_embedding_l2_distance(&entity->embedding,
                                            &agg.mean_embedding);

        /* High distance = anomaly */
        if (score > threshold) {
            entity->flags |= TL_ENTITY_FLAG_ANOMALOUS;
            result->total_matches++;
            if (result->count < 64) {
                result->handles[result->count++] = entity->atom;
            }
        } else {
            entity->flags &= ~TL_ENTITY_FLAG_ANOMALOUS;
        }
    }

    ctx->queries++;
    simple_unlock(&ctx->lock);
    return KERN_SUCCESS;
}

tensor_scalar_t
tl_anomaly_score(tensor_logic_context_t ctx, atom_handle_t entity)
{
    tl_entity_t e;
    struct tl_scale_aggregate agg;
    tensor_scalar_t score = 0;

    if (ctx == NULL)
        return 0;

    simple_lock(&ctx->lock);

    e = tl_find_entity(ctx, entity);
    if (e != NULL) {
        simple_unlock(&ctx->lock);
        tl_aggregate_scale(ctx, e->scale, &agg);
        simple_lock(&ctx->lock);

        score = tensor_embedding_l2_distance(&e->embedding,
                                            &agg.mean_embedding);
    }

    simple_unlock(&ctx->lock);
    return score;
}

/*
 * ============================================================
 * STATISTICS AND DEBUGGING
 * ============================================================
 */

void
tl_get_stats(tensor_logic_context_t ctx, uint32_t *entity_count,
             uint32_t *relationship_count, uint32_t *cluster_count)
{
    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);

    if (entity_count != NULL)
        *entity_count = ctx->entity_count;

    if (relationship_count != NULL)
        *relationship_count = atomspace_get_link_count(ctx->atomspace);

    if (cluster_count != NULL)
        *cluster_count = ctx->cluster_count;

    simple_unlock(&ctx->lock);
}

void
tl_print_stats(tensor_logic_context_t ctx)
{
    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);

    printf("tensor_logic: entities=%d clusters=%d\n",
           ctx->entity_count, ctx->cluster_count);
    printf("  updates=%d queries=%d inferences=%d\n",
           ctx->updates, ctx->queries, ctx->inferences);
    printf("  atomspace: ");
    atomspace_print_stats(ctx->atomspace);

    simple_unlock(&ctx->lock);
}

void
tl_print_entity(tensor_logic_context_t ctx, atom_handle_t handle)
{
    tl_entity_t entity;
    atom_t atom;

    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);

    entity = tl_find_entity(ctx, handle);
    if (entity == NULL) {
        printf("tensor_logic: entity not found (handle=%d)\n", handle);
        simple_unlock(&ctx->lock);
        return;
    }

    atom = atomspace_get_atom(ctx->atomspace, handle);

    printf("tensor_logic entity:\n");
    printf("  handle=%d category=%s scale=%d\n",
           handle, category_names[entity->category], entity->scale);
    printf("  kernel_id=%d ptr=%p flags=0x%x\n",
           entity->kernel_id, entity->kernel_ptr, entity->flags);

    if (atom != NULL) {
        printf("  atom: name=%s type=%d\n", atom->name, atom->type);
        printf("  tv: strength=%d conf=%d\n",
               atom->tv.strength, atom->tv.confidence);
        printf("  av: sti=%d lti=%d\n", atom->av.sti, atom->av.lti);
    }

    simple_unlock(&ctx->lock);
}

void
tl_print_clusters(tensor_logic_context_t ctx)
{
    uint32_t i;

    if (ctx == NULL)
        return;

    simple_lock(&ctx->lock);

    printf("tensor_logic clusters: %d total\n", ctx->cluster_count);
    for (i = 0; i < ctx->cluster_count; i++) {
        tl_cluster_t cluster = &ctx->clusters[i];
        printf("  cluster %d: category=%s count=%d coherence=%d\n",
               cluster->cluster_id,
               category_names[cluster->category],
               cluster->count,
               cluster->coherence);
    }

    simple_unlock(&ctx->lock);
}
