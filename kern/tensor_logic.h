/*
 * GNU Mach Tensor Logic Subsystem
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
 * tensor_logic.h - Tensor Logic Integration Layer
 *
 * This module integrates:
 *   - ATen-inspired tensor operations (kern/tensor.h)
 *   - AtomSpace-inspired hypergraph (kern/atomspace.h)
 *
 * Together they provide "Tensor Logic" for:
 *   - Multi-Entity tracking (tasks, threads, ports, processors)
 *   - Multi-Scale analysis (thread → task → system level)
 *   - Network-aware operations (IPC relationship modeling)
 *   - Tensor-enhanced semantics (neural embeddings for entities)
 *
 * Inspired by:
 *   - ATen (https://github.com/o9nn/ATen)
 *   - ATenSpace (https://github.com/o9nn/ATenSpace)
 *   - OpenCog AtomSpace (https://opencog.org/)
 */

#ifndef _KERN_TENSOR_LOGIC_H_
#define _KERN_TENSOR_LOGIC_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <kern/tensor.h>
#include <kern/atomspace.h>

/*
 * Tensor Logic configuration
 */
#define TL_MAX_ENTITIES         512     /* Maximum tracked entities */
#define TL_MAX_SCALE_LEVELS     4       /* Hierarchy levels */
#define TL_SIMILARITY_THRESHOLD (TENSOR_FIXED_POINT_ONE / 2)  /* 0.5 */
#define TL_ATTENTION_THRESHOLD  50      /* STI threshold for focus */

/*
 * Entity scale levels - for multi-scale analysis
 */
typedef enum {
    TL_SCALE_THREAD     = 0,    /* Individual thread level */
    TL_SCALE_TASK       = 1,    /* Task/process level */
    TL_SCALE_HOST       = 2,    /* Single machine level */
    TL_SCALE_NETWORK    = 3,    /* Distributed network level */
} tl_scale_t;

/*
 * Entity categories for multi-entity tracking
 */
typedef enum {
    TL_ENTITY_TASK,
    TL_ENTITY_THREAD,
    TL_ENTITY_PORT,
    TL_ENTITY_PROCESSOR,
    TL_ENTITY_MEMORY,
    TL_ENTITY_DEVICE,
    TL_ENTITY_NETWORK,
    TL_ENTITY_COUNT
} tl_entity_category_t;

/*
 * Relationship types for network-aware modeling
 */
typedef enum {
    TL_REL_CONTAINS,        /* Task contains threads */
    TL_REL_COMMUNICATES,    /* IPC communication */
    TL_REL_DEPENDS,         /* Resource dependency */
    TL_REL_COMPETES,        /* Resource competition */
    TL_REL_AFFINITY,        /* CPU affinity */
    TL_REL_SIMILAR,         /* Semantic similarity */
    TL_REL_TEMPORAL,        /* Temporal ordering */
} tl_relationship_t;

/*
 * Network topology types
 */
typedef enum {
    TL_TOPO_UNKNOWN,
    TL_TOPO_STAR,           /* Central hub pattern */
    TL_TOPO_MESH,           /* Fully connected */
    TL_TOPO_RING,           /* Ring communication */
    TL_TOPO_TREE,           /* Hierarchical tree */
    TL_TOPO_PIPELINE,       /* Linear pipeline */
} tl_topology_t;

/*
 * Entity descriptor - combines atom with tensor metadata
 */
struct tl_entity {
    atom_handle_t           atom;           /* AtomSpace atom */
    tl_entity_category_t    category;       /* Entity category */
    tl_scale_t              scale;          /* Hierarchy scale */
    uint32_t                kernel_id;      /* Kernel object ID */
    void                   *kernel_ptr;     /* Kernel object pointer */
    struct tensor_embedding embedding;      /* Neural embedding */
    uint32_t                flags;
};

typedef struct tl_entity *tl_entity_t;

#define TL_ENTITY_FLAG_ACTIVE       0x0001
#define TL_ENTITY_FLAG_MONITORED    0x0002
#define TL_ENTITY_FLAG_CLUSTERED    0x0004
#define TL_ENTITY_FLAG_ANOMALOUS    0x0008

/*
 * Entity cluster - group of similar entities
 */
struct tl_cluster {
    atom_handle_t           atoms[64];      /* Cluster members */
    uint32_t                count;          /* Number of members */
    struct tensor_embedding centroid;       /* Cluster centroid */
    tensor_scalar_t         coherence;      /* Internal similarity */
    tl_entity_category_t    category;       /* Primary category */
    uint32_t                cluster_id;
};

typedef struct tl_cluster *tl_cluster_t;

/*
 * Network analysis result
 */
struct tl_network_analysis {
    tl_topology_t           topology;       /* Detected topology */
    uint32_t                node_count;     /* Network nodes */
    uint32_t                edge_count;     /* Network edges */
    uint32_t                component_count; /* Connected components */
    tensor_scalar_t         density;        /* Edge density */
    tensor_scalar_t         clustering;     /* Clustering coefficient */
    atom_handle_t           hubs[8];        /* Central hubs */
    uint32_t                hub_count;
};

typedef struct tl_network_analysis *tl_network_analysis_t;

/*
 * Scale aggregation result
 */
struct tl_scale_aggregate {
    tl_scale_t              scale;
    uint32_t                entity_count;
    struct tensor_embedding mean_embedding;
    tensor_scalar_t         variance;
    int32_t                 total_attention;
    atom_handle_t           representative;  /* Most representative entity */
};

typedef struct tl_scale_aggregate *tl_scale_aggregate_t;

/*
 * Tensor Logic context - main subsystem state
 */
struct tensor_logic_context {
    atomspace_t             atomspace;      /* Knowledge hypergraph */

    /* Entity tracking */
    struct tl_entity        entities[TL_MAX_ENTITIES];
    uint32_t                entity_count;
    uint32_t                next_entity_id;

    /* Index by category */
    atom_handle_t           category_roots[TL_ENTITY_COUNT];

    /* Clustering */
    struct tl_cluster       clusters[32];
    uint32_t                cluster_count;

    /* Statistics */
    uint32_t                updates;
    uint32_t                queries;
    uint32_t                inferences;

    /* Synchronization */
    decl_simple_lock_data(, lock)

    /* Configuration */
    uint32_t                flags;
    tensor_scalar_t         similarity_threshold;
    int16_t                 attention_threshold;
};

typedef struct tensor_logic_context *tensor_logic_context_t;

/*
 * Context flags
 */
#define TL_FLAG_INITIALIZED     0x0001
#define TL_FLAG_AUTO_CLUSTER    0x0002
#define TL_FLAG_AUTO_EMBED      0x0004
#define TL_FLAG_TRACK_IPC       0x0008
#define TL_FLAG_NETWORK_AWARE   0x0010

/*
 * ============================================================
 * TENSOR LOGIC LIFECYCLE
 * ============================================================
 */

/*
 * Initialize the global Tensor Logic subsystem
 */
void tensor_logic_init(void);

/*
 * Create a new Tensor Logic context
 */
kern_return_t tensor_logic_create(tensor_logic_context_t *ctx);

/*
 * Destroy a Tensor Logic context
 */
void tensor_logic_destroy(tensor_logic_context_t ctx);

/*
 * Get the default global context
 */
tensor_logic_context_t tensor_logic_get_default(void);

/*
 * ============================================================
 * MULTI-ENTITY TRACKING
 * ============================================================
 */

/*
 * Register a kernel entity for tracking
 */
kern_return_t tl_register_task(tensor_logic_context_t ctx,
                               void *task, const char *name,
                               atom_handle_t *handle);

kern_return_t tl_register_thread(tensor_logic_context_t ctx,
                                 void *thread, atom_handle_t task,
                                 const char *name, atom_handle_t *handle);

kern_return_t tl_register_port(tensor_logic_context_t ctx,
                               void *port, atom_handle_t owner,
                               const char *name, atom_handle_t *handle);

kern_return_t tl_register_processor(tensor_logic_context_t ctx,
                                    void *processor, int cpu_id,
                                    atom_handle_t *handle);

/*
 * Unregister an entity
 */
kern_return_t tl_unregister_entity(tensor_logic_context_t ctx,
                                   atom_handle_t handle);

/*
 * Update entity embedding based on behavior
 */
kern_return_t tl_update_embedding(tensor_logic_context_t ctx,
                                  atom_handle_t handle,
                                  tensor_embedding_t new_embed);

/*
 * Update entity attention based on activity
 */
void tl_stimulate_entity(tensor_logic_context_t ctx,
                         atom_handle_t handle, int16_t amount);

/*
 * ============================================================
 * NETWORK-AWARE OPERATIONS
 * ============================================================
 */

/*
 * Record IPC communication between entities
 */
kern_return_t tl_record_ipc(tensor_logic_context_t ctx,
                            atom_handle_t sender, atom_handle_t receiver,
                            uint32_t message_size);

/*
 * Record resource dependency
 */
kern_return_t tl_record_dependency(tensor_logic_context_t ctx,
                                   atom_handle_t dependent,
                                   atom_handle_t resource);

/*
 * Record CPU affinity
 */
kern_return_t tl_record_affinity(tensor_logic_context_t ctx,
                                 atom_handle_t thread,
                                 atom_handle_t processor);

/*
 * Analyze IPC network topology
 */
kern_return_t tl_analyze_network(tensor_logic_context_t ctx,
                                 tl_entity_category_t category,
                                 struct tl_network_analysis *result);

/*
 * Find communication paths between entities
 */
kern_return_t tl_find_path(tensor_logic_context_t ctx,
                           atom_handle_t source, atom_handle_t target,
                           atom_handle_t *path, uint32_t *path_len);

/*
 * Identify communication hubs
 */
kern_return_t tl_find_hubs(tensor_logic_context_t ctx,
                           tl_entity_category_t category,
                           atom_query_result_t result);

/*
 * ============================================================
 * MULTI-SCALE ANALYSIS
 * ============================================================
 */

/*
 * Aggregate entities at a given scale
 */
kern_return_t tl_aggregate_scale(tensor_logic_context_t ctx,
                                 tl_scale_t scale,
                                 struct tl_scale_aggregate *result);

/*
 * Get entities at a specific scale level
 */
kern_return_t tl_get_scale_entities(tensor_logic_context_t ctx,
                                    tl_scale_t scale,
                                    atom_query_result_t result);

/*
 * Propagate information up the hierarchy
 */
kern_return_t tl_propagate_up(tensor_logic_context_t ctx,
                              atom_handle_t entity);

/*
 * Propagate information down the hierarchy
 */
kern_return_t tl_propagate_down(tensor_logic_context_t ctx,
                                atom_handle_t entity);

/*
 * ============================================================
 * TENSOR-ENHANCED SEMANTICS
 * ============================================================
 */

/*
 * Find entities semantically similar to target
 */
kern_return_t tl_find_similar(tensor_logic_context_t ctx,
                              atom_handle_t target,
                              tensor_scalar_t threshold,
                              atom_query_result_t result);

/*
 * Compute semantic similarity between entities
 */
tensor_scalar_t tl_similarity(tensor_logic_context_t ctx,
                              atom_handle_t a, atom_handle_t b);

/*
 * Cluster entities by embedding similarity
 */
kern_return_t tl_cluster_entities(tensor_logic_context_t ctx,
                                  tl_entity_category_t category);

/*
 * Get cluster for an entity
 */
tl_cluster_t tl_get_cluster(tensor_logic_context_t ctx,
                            atom_handle_t entity);

/*
 * Compute cluster centroid
 */
kern_return_t tl_compute_centroid(tensor_logic_context_t ctx,
                                  tl_cluster_t cluster);

/*
 * ============================================================
 * ATTENTION AND FOCUS
 * ============================================================
 */

/*
 * Get entities in attentional focus
 */
kern_return_t tl_get_focus(tensor_logic_context_t ctx,
                           uint32_t count,
                           atom_query_result_t result);

/*
 * Run attention spreading pass
 */
void tl_spread_attention(tensor_logic_context_t ctx);

/*
 * Run attention decay pass
 */
void tl_decay_attention(tensor_logic_context_t ctx);

/*
 * ============================================================
 * INFERENCE AND QUERIES
 * ============================================================
 */

/*
 * Query for entities matching criteria
 */
kern_return_t tl_query(tensor_logic_context_t ctx,
                       tl_entity_category_t category,
                       atom_predicate_t predicate,
                       void *pred_context,
                       atom_query_result_t result);

/*
 * Check inheritance relationship
 */
boolean_t tl_is_a(tensor_logic_context_t ctx,
                  atom_handle_t entity, atom_handle_t type);

/*
 * Get all relationships for an entity
 */
kern_return_t tl_get_relationships(tensor_logic_context_t ctx,
                                   atom_handle_t entity,
                                   tl_relationship_t rel_type,
                                   atom_query_result_t result);

/*
 * ============================================================
 * ANOMALY DETECTION
 * ============================================================
 */

/*
 * Detect anomalous entities based on embedding deviation
 */
kern_return_t tl_detect_anomalies(tensor_logic_context_t ctx,
                                  tl_entity_category_t category,
                                  tensor_scalar_t threshold,
                                  atom_query_result_t result);

/*
 * Get entity anomaly score
 */
tensor_scalar_t tl_anomaly_score(tensor_logic_context_t ctx,
                                 atom_handle_t entity);

/*
 * ============================================================
 * STATISTICS AND DEBUGGING
 * ============================================================
 */

/*
 * Get subsystem statistics
 */
void tl_get_stats(tensor_logic_context_t ctx,
                  uint32_t *entity_count,
                  uint32_t *relationship_count,
                  uint32_t *cluster_count);

/*
 * Print debug information
 */
void tl_print_stats(tensor_logic_context_t ctx);

/*
 * Print entity details
 */
void tl_print_entity(tensor_logic_context_t ctx, atom_handle_t handle);

/*
 * Dump cluster information
 */
void tl_print_clusters(tensor_logic_context_t ctx);

#endif /* _KERN_TENSOR_LOGIC_H_ */
