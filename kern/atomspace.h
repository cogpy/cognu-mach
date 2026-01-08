/*
 * GNU Mach AtomSpace-Inspired Hypergraph Subsystem
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * Inspired by OpenCog AtomSpace (https://github.com/opencog/atomspace)
 * and ATenSpace (https://github.com/o9nn/ATenSpace)
 *
 * This provides a lightweight hypergraph knowledge representation
 * for kernel-level cognitive computing and multi-entity tracking.
 */

#ifndef _KERN_ATOMSPACE_H_
#define _KERN_ATOMSPACE_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/tensor.h>

/*
 * AtomSpace configuration
 */
#define ATOM_MAX_NAME_LEN       32      /* Maximum atom name length */
#define ATOM_MAX_OUTGOING       8       /* Maximum outgoing set size */
#define ATOM_HASH_BUCKETS       256     /* Hash table buckets */
#define ATOMSPACE_MAX_ATOMS     1024    /* Maximum atoms per atomspace */

/*
 * Atom types - kernel entity representation
 * Nodes represent entities, Links represent relationships
 */
typedef enum {
    /* Node types (entities) */
    ATOM_TYPE_NODE              = 0,    /* Base node type */
    ATOM_TYPE_CONCEPT_NODE      = 1,    /* Abstract concept */
    ATOM_TYPE_TASK_NODE         = 2,    /* Kernel task */
    ATOM_TYPE_THREAD_NODE       = 3,    /* Kernel thread */
    ATOM_TYPE_PORT_NODE         = 4,    /* IPC port */
    ATOM_TYPE_PROCESSOR_NODE    = 5,    /* CPU processor */
    ATOM_TYPE_MEMORY_NODE       = 6,    /* Memory region */
    ATOM_TYPE_DEVICE_NODE       = 7,    /* Device */
    ATOM_TYPE_NETWORK_NODE      = 8,    /* Network endpoint */

    /* Link types (relationships) */
    ATOM_TYPE_LINK              = 64,   /* Base link type */
    ATOM_TYPE_INHERITANCE_LINK  = 65,   /* IS-A relationship */
    ATOM_TYPE_SIMILARITY_LINK   = 66,   /* Semantic similarity */
    ATOM_TYPE_EXECUTION_LINK    = 67,   /* Execution relationship */
    ATOM_TYPE_MEMBER_LINK       = 68,   /* Set membership */
    ATOM_TYPE_IPC_LINK          = 69,   /* IPC connection */
    ATOM_TYPE_DEPENDS_LINK      = 70,   /* Dependency */
    ATOM_TYPE_AFFINITY_LINK     = 71,   /* CPU affinity */
    ATOM_TYPE_TEMPORAL_LINK     = 72,   /* Temporal ordering */
} atom_type_t;

#define ATOM_IS_NODE(type)  ((type) < ATOM_TYPE_LINK)
#define ATOM_IS_LINK(type)  ((type) >= ATOM_TYPE_LINK)

/*
 * Truth value - probabilistic confidence
 * Represents uncertain or fuzzy relationships
 */
struct truth_value {
    tensor_scalar_t strength;       /* Mean value [0, 1] */
    tensor_scalar_t confidence;     /* Confidence [0, 1] */
    tensor_scalar_t count;          /* Evidence count */
};

typedef struct truth_value *truth_value_t;

/* Common truth value constants */
#define TV_TRUE_STRENGTH    TENSOR_FIXED_POINT_ONE
#define TV_FALSE_STRENGTH   0
#define TV_DEFAULT_CONF     (TENSOR_FIXED_POINT_ONE / 10)

/*
 * Attention value - importance allocation
 * Used for ECAN (Economic Attention Networks)
 */
struct attention_value {
    int16_t sti;                    /* Short-term importance */
    int16_t lti;                    /* Long-term importance */
    int16_t vlti;                   /* Very long-term importance */
};

typedef struct attention_value *attention_value_t;

#define AV_DEFAULT_STI      0
#define AV_DEFAULT_LTI      0
#define AV_IMPORTANT_STI    100
#define AV_FORGETTABLE_STI  (-100)

/*
 * Atom handle - unique identifier
 */
typedef uint32_t atom_handle_t;
#define ATOM_HANDLE_INVALID 0

/*
 * Atom structure - fundamental knowledge unit
 * Can be either a Node (entity) or Link (relationship)
 */
struct atom {
    /* Identity */
    atom_handle_t       handle;         /* Unique handle */
    atom_type_t         type;           /* Atom type */
    uint32_t            hash;           /* Hash for lookup */

    /* Node-specific: name */
    char                name[ATOM_MAX_NAME_LEN];

    /* Link-specific: outgoing set */
    atom_handle_t       outgoing[ATOM_MAX_OUTGOING];
    uint8_t             arity;          /* Number of outgoing atoms */

    /* Associated values */
    struct truth_value  tv;             /* Truth value */
    struct attention_value av;          /* Attention value */

    /* Tensor embedding */
    struct tensor_embedding embedding;  /* Neural embedding */

    /* Kernel entity binding */
    void               *kernel_object;  /* Pointer to kernel object */
    uint32_t            entity_id;      /* Entity identifier */

    /* Metadata */
    uint32_t            timestamp;      /* Creation/update time */
    uint32_t            refcount;       /* Reference count */
    uint16_t            flags;          /* Atom flags */

    /* Linking */
    queue_chain_t       hash_chain;     /* Hash bucket chain */
    queue_chain_t       type_chain;     /* Type index chain */

    decl_simple_lock_data(, lock)       /* Atom lock */
};

typedef struct atom *atom_t;

/*
 * Atom flags
 */
#define ATOM_FLAG_VALID         0x0001  /* Atom is valid */
#define ATOM_FLAG_IMMUTABLE     0x0002  /* Atom is immutable */
#define ATOM_FLAG_REMOVED       0x0004  /* Atom marked for removal */
#define ATOM_FLAG_HAS_EMBED     0x0008  /* Has valid embedding */
#define ATOM_FLAG_KERNEL_BOUND  0x0010  /* Bound to kernel object */

/*
 * AtomSpace - hypergraph container
 */
struct atomspace {
    /* Storage */
    struct atom         atoms[ATOMSPACE_MAX_ATOMS];
    uint32_t            atom_count;
    atom_handle_t       next_handle;

    /* Hash table for lookup */
    queue_head_t        hash_table[ATOM_HASH_BUCKETS];

    /* Type index for efficient type queries */
    queue_head_t        type_index[128];

    /* Attention bank */
    int32_t             attention_funds;    /* Total STI funds */
    int32_t             wage;               /* STI wage for atoms */

    /* Statistics */
    uint32_t            node_count;
    uint32_t            link_count;
    uint32_t            query_count;

    /* Synchronization */
    decl_simple_lock_data(, lock)

    /* Metadata */
    uint32_t            version;
    uint32_t            flags;
};

typedef struct atomspace *atomspace_t;

/*
 * AtomSpace flags
 */
#define ATOMSPACE_FLAG_READONLY     0x0001
#define ATOMSPACE_FLAG_PERSISTENT   0x0002

/*
 * Query result structure
 */
struct atom_query_result {
    atom_handle_t       handles[64];    /* Result handles */
    uint32_t            count;          /* Number of results */
    uint32_t            total_matches;  /* Total matches (may exceed returned) */
};

typedef struct atom_query_result *atom_query_result_t;

/*
 * AtomSpace lifecycle
 */
kern_return_t atomspace_create(atomspace_t *result);
void atomspace_destroy(atomspace_t as);
void atomspace_clear(atomspace_t as);

/*
 * Atom creation
 */
atom_handle_t atomspace_add_node(atomspace_t as, atom_type_t type,
                                 const char *name);
atom_handle_t atomspace_add_link(atomspace_t as, atom_type_t type,
                                 atom_handle_t *outgoing, uint8_t arity);
atom_t atomspace_get_atom(atomspace_t as, atom_handle_t handle);
kern_return_t atomspace_remove_atom(atomspace_t as, atom_handle_t handle);

/*
 * Atom lookup
 */
atom_handle_t atomspace_find_node(atomspace_t as, atom_type_t type,
                                  const char *name);
kern_return_t atomspace_find_by_type(atomspace_t as, atom_type_t type,
                                     atom_query_result_t result);
kern_return_t atomspace_find_incoming(atomspace_t as, atom_handle_t target,
                                      atom_query_result_t result);

/*
 * Truth value operations
 */
void atom_set_truth_value(atom_t a, tensor_scalar_t strength,
                         tensor_scalar_t confidence);
void atom_get_truth_value(atom_t a, tensor_scalar_t *strength,
                         tensor_scalar_t *confidence);
tensor_scalar_t atom_get_mean(atom_t a);

/*
 * Attention value operations
 */
void atom_set_attention(atom_t a, int16_t sti, int16_t lti);
void atom_stimulate(atom_t a, int16_t amount);
void atom_decay_attention(atom_t a);

/*
 * Embedding operations
 */
kern_return_t atom_set_embedding(atom_t a, tensor_embedding_t embed);
kern_return_t atom_get_embedding(atom_t a, tensor_embedding_t embed);
kern_return_t atom_compute_embedding(atomspace_t as, atom_t a);

/*
 * Semantic similarity
 */
tensor_scalar_t atomspace_similarity(atomspace_t as,
                                    atom_handle_t a, atom_handle_t b);
kern_return_t atomspace_find_similar(atomspace_t as, atom_handle_t target,
                                     tensor_scalar_t threshold,
                                     atom_query_result_t result);

/*
 * Kernel entity binding
 */
kern_return_t atom_bind_task(atom_t a, void *task);
kern_return_t atom_bind_thread(atom_t a, void *thread);
kern_return_t atom_bind_port(atom_t a, void *port);
kern_return_t atom_bind_processor(atom_t a, void *processor);

/*
 * Pattern matching (simplified)
 */
typedef boolean_t (*atom_predicate_t)(atom_t a, void *context);
kern_return_t atomspace_filter(atomspace_t as, atom_predicate_t pred,
                              void *context, atom_query_result_t result);

/*
 * Inference helpers
 */
kern_return_t atomspace_get_inheritance(atomspace_t as, atom_handle_t child,
                                        atom_query_result_t parents);
boolean_t atomspace_inherits_from(atomspace_t as, atom_handle_t child,
                                  atom_handle_t parent);

/*
 * ECAN attention allocation
 */
void atomspace_spread_attention(atomspace_t as);
void atomspace_decay_attention(atomspace_t as);
kern_return_t atomspace_get_attentional_focus(atomspace_t as, uint32_t count,
                                              atom_query_result_t result);

/*
 * Statistics and debugging
 */
uint32_t atomspace_get_size(atomspace_t as);
uint32_t atomspace_get_node_count(atomspace_t as);
uint32_t atomspace_get_link_count(atomspace_t as);
void atomspace_print_stats(atomspace_t as);

/*
 * Subsystem initialization
 */
void atomspace_subsystem_init(void);

#endif /* _KERN_ATOMSPACE_H_ */
