/*
 * GNU Mach Ontogenesis - Self-Generating Kernel System
 * Copyright (c) 2024-2025 Free Software Foundation, Inc.
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
 * ontogenesis.h - Self-Generating Kernel System
 *
 * Cherry-picked from ggnumlcash.cpp ontogenesis commits and ported
 * to kernel C, implementing the behavioral contract defined in
 * gnucashmulti/.github/agents/ONTOGENESIS.md.
 *
 * Ontogenesis enables kernels to:
 *   1. Self-Generate: Create new kernels through recursive self-composition
 *   2. Self-Optimize: Improve grip through iterative gradient ascent
 *   3. Self-Reproduce: Combine two kernels via crossover/mutation/cloning
 *   4. Evolve: Populations evolve over generations to maximize fitness
 *
 * Based on B-series expansions and differential operators.
 * Uses fixed-point arithmetic from kern/tensor.h for kernel safety.
 *
 * Mirrors AtomSpace's ability to dynamically add atom types,
 * here applied to self-modifying computational kernels.
 *
 * See also:
 *   - kern/tensor.h      (fixed-point tensor operations)
 *   - kern/atomspace.h    (hypergraph knowledge representation)
 *   - kern/tensor_logic.h (tensor + atomspace integration)
 */

#ifndef _KERN_ONTOGENESIS_H_
#define _KERN_ONTOGENESIS_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <kern/lock.h>
#include <kern/tensor.h>

/*
 * Ontogenesis configuration constants
 */
#define ONTO_MAX_COEFFICIENTS   16      /* Maximum B-series coefficients */
#define ONTO_MAX_GENES          8       /* Maximum genes per genome */
#define ONTO_MAX_GENE_VALUES    16      /* Maximum values per gene */
#define ONTO_MAX_LINEAGE        8       /* Maximum parent IDs tracked */
#define ONTO_MAX_HISTORY        64      /* Maximum development events */
#define ONTO_MAX_POPULATION     64      /* Maximum population size */
#define ONTO_ID_LEN             32      /* Kernel ID string length */
#define ONTO_DESC_LEN           64      /* Description string length */

/*
 * Fixed-point conversion helpers for ontogenesis
 * Reuse tensor.h fixed-point infrastructure
 */
#define ONTO_FIXED_ONE          TENSOR_FIXED_POINT_ONE
#define ONTO_FIXED_HALF         (TENSOR_FIXED_POINT_ONE / 2)
#define ONTO_FIXED_TENTH        (TENSOR_FIXED_POINT_ONE / 10)
#define ONTO_FIXED_MUL(a, b)    TENSOR_FIXED_MUL(a, b)
#define ONTO_FIXED_DIV(a, b)    TENSOR_FIXED_DIV(a, b)

/*
 * Development stages of a kernel (per ONTOGENESIS.md spec)
 *   - Embryonic: Just generated, basic structure
 *   - Juvenile:  Developing, optimizing
 *   - Mature:    Fully developed, capable of reproduction
 *   - Senescent: Declining, ready for replacement
 */
typedef enum {
	ONTO_STAGE_EMBRYONIC = 0,
	ONTO_STAGE_JUVENILE  = 1,
	ONTO_STAGE_MATURE    = 2,
	ONTO_STAGE_SENESCENT = 3,
} onto_stage_t;

/*
 * Gene types in kernel genome (per ONTOGENESIS.md spec)
 *   - Coefficient: B-series coefficients (mutable)
 *   - Operator:    Differential operators (mutable)
 *   - Symmetry:    Domain symmetries (immutable)
 *   - Preservation: Conserved quantities (immutable)
 */
typedef enum {
	ONTO_GENE_COEFFICIENT  = 0,
	ONTO_GENE_OPERATOR     = 1,
	ONTO_GENE_SYMMETRY     = 2,
	ONTO_GENE_PRESERVATION = 3,
} onto_gene_type_t;

/*
 * Reproduction methods (per ONTOGENESIS.md spec)
 *   - Crossover: Single-point genetic crossover
 *   - Mutation:  Random coefficient mutation
 *   - Cloning:   Direct copy
 */
typedef enum {
	ONTO_REPRO_CROSSOVER = 0,
	ONTO_REPRO_MUTATION  = 1,
	ONTO_REPRO_CLONING   = 2,
} onto_reproduction_t;

/*
 * A single gene in the kernel genome
 */
struct onto_gene {
	onto_gene_type_t    type;
	tensor_scalar_t     values[ONTO_MAX_GENE_VALUES];
	uint16_t            num_values;
	boolean_t           is_mutable;
};

/*
 * Genetic information of a kernel - the "DNA"
 */
struct onto_genome {
	char                id[ONTO_ID_LEN];
	int                 generation;
	char                lineage[ONTO_MAX_LINEAGE][ONTO_ID_LEN];
	uint16_t            lineage_count;
	struct onto_gene    genes[ONTO_MAX_GENES];
	uint16_t            gene_count;
	tensor_scalar_t     fitness;        /* Fixed-point [0, 1] */
	int                 age;
};

/*
 * Development state tracking
 */
struct onto_state {
	onto_stage_t        stage;
	tensor_scalar_t     maturity;       /* Fixed-point [0, 1] */
	int                 development_cycles;
};

/*
 * Development event history entry
 */
struct onto_event {
	int                 iteration;
	tensor_scalar_t     grip;           /* Fixed-point grip value */
	onto_stage_t        stage;
	char                description[ONTO_DESC_LEN];
};

/*
 * Core ontogenetic kernel structure
 * An enhanced kernel with genetic capabilities
 */
struct onto_kernel {
	struct onto_genome  genome;
	struct onto_state   state;
	struct onto_event   history[ONTO_MAX_HISTORY];
	uint16_t            history_count;

	/* B-series coefficients (the actual computational kernel) */
	tensor_scalar_t     coefficients[ONTO_MAX_COEFFICIENTS];
	uint16_t            num_coefficients;
	int                 order;

	decl_simple_lock_data(, lock)
};

typedef struct onto_kernel *onto_kernel_t;

/*
 * Grip metrics for fitness evaluation
 * (per ONTOGENESIS.md spec: fitness = grip*0.4 + stability*0.2
 *  + efficiency*0.2 + novelty*0.1 + symmetry*0.1)
 */
struct onto_grip {
	tensor_scalar_t     contact;        /* How well kernel touches domain */
	tensor_scalar_t     coverage;       /* Completeness of span */
	tensor_scalar_t     efficiency;     /* Computational cost */
	tensor_scalar_t     stability;      /* Numerical properties */
	tensor_scalar_t     novelty;        /* Genetic diversity */
	tensor_scalar_t     symmetry;       /* Symmetry preservation */
};

/*
 * Evolution configuration (per ONTOGENESIS.md spec defaults)
 */
struct onto_evolution_config {
	int                 population_size;
	tensor_scalar_t     mutation_rate;      /* Fixed-point [0, 1] */
	tensor_scalar_t     crossover_rate;     /* Fixed-point [0, 1] */
	tensor_scalar_t     elitism_rate;       /* Fixed-point [0, 1] */
	int                 max_generations;
	tensor_scalar_t     fitness_threshold;  /* Fixed-point [0, 1] */
	tensor_scalar_t     diversity_pressure; /* Fixed-point [0, 1] */
};

/*
 * Development schedule configuration (per ONTOGENESIS.md spec)
 */
struct onto_dev_schedule {
	int                 embryonic_duration; /* Generations */
	int                 juvenile_duration;  /* Generations */
	int                 mature_duration;    /* Generations */
	tensor_scalar_t     maturity_threshold; /* Fixed-point [0, 1] */
};

/*
 * Complete ontogenesis configuration
 */
struct onto_config {
	struct onto_evolution_config evolution;
	struct onto_dev_schedule     development;
	struct onto_kernel           seed_kernels[ONTO_MAX_POPULATION];
	uint16_t                     seed_count;
};

/*
 * Generation statistics
 */
struct onto_gen_stats {
	int                 generation;
	tensor_scalar_t     best_fitness;
	tensor_scalar_t     average_fitness;
	tensor_scalar_t     worst_fitness;
	tensor_scalar_t     diversity;
	int                 population_size;
};

/* === Initialization === */

/*
 * Initialize the ontogenesis subsystem
 */
extern void ontogenesis_init(void);

/*
 * Initialize an ontogenetic kernel from coefficients
 * Corresponds to: initializeOntogeneticKernel() in spec
 */
extern kern_return_t onto_kernel_init(
	struct onto_kernel *kernel,
	const tensor_scalar_t *coefficients,
	uint16_t num_coefficients,
	int order);

/*
 * Initialize default evolution configuration
 */
extern void onto_config_default(struct onto_config *config);

/* === Core Ontogenesis Operations === */

/*
 * Self-generate: Create offspring through recursive self-composition
 * Applies chain rule: (f∘f)' = f'(f(x)) · f'(x)
 * Corresponds to: selfGenerate() in spec
 */
extern kern_return_t onto_self_generate(
	const struct onto_kernel *parent,
	struct onto_kernel *offspring);

/*
 * Self-optimize: Improve grip through iterative gradient ascent
 * Corresponds to: selfOptimize() in spec
 */
extern kern_return_t onto_self_optimize(
	struct onto_kernel *kernel,
	int iterations);

/*
 * Self-reproduce: Combine two kernels to create offspring
 * Corresponds to: selfReproduce() in spec
 */
extern kern_return_t onto_self_reproduce(
	const struct onto_kernel *parent1,
	const struct onto_kernel *parent2,
	onto_reproduction_t method,
	struct onto_kernel *offspring);

/* === Genetic Operations === */

/*
 * Crossover two genomes (single-point crossover)
 */
extern kern_return_t onto_crossover(
	const struct onto_genome *g1,
	const struct onto_genome *g2,
	struct onto_genome *offspring);

/*
 * Mutate a genome
 */
extern void onto_mutate(
	struct onto_genome *genome,
	tensor_scalar_t mutation_rate);

/* === Fitness Evaluation === */

/*
 * Evaluate grip metrics for a kernel
 */
extern struct onto_grip onto_evaluate_grip(
	const struct onto_kernel *kernel);

/*
 * Calculate total grip score (weighted combination per spec)
 */
extern tensor_scalar_t onto_grip_total(
	const struct onto_grip *grip);

/*
 * Calculate genetic distance between two kernels
 */
extern tensor_scalar_t onto_genetic_distance(
	const struct onto_kernel *k1,
	const struct onto_kernel *k2);

/*
 * Calculate fitness including population diversity
 */
extern tensor_scalar_t onto_calculate_fitness(
	const struct onto_kernel *kernel,
	const struct onto_kernel *population,
	int population_size);

/* === Evolution === */

/*
 * Run complete ontogenesis evolution
 * Returns number of generations completed via out param
 * Corresponds to: runOntogenesis() in spec
 */
extern kern_return_t onto_run_evolution(
	const struct onto_config *config,
	struct onto_gen_stats *stats,
	int *num_generations);

/*
 * Evolve a single generation
 */
extern void onto_evolve_generation(
	struct onto_kernel *population,
	int *population_size,
	const struct onto_evolution_config *config);

/*
 * Update development stages based on age and maturity
 */
extern void onto_update_stages(
	struct onto_kernel *population,
	int population_size,
	const struct onto_dev_schedule *schedule);

/* === Utility === */

/*
 * Generate a unique kernel ID
 */
extern void onto_generate_id(char *buf, int buflen);

/*
 * Calculate population diversity (average genetic distance)
 */
extern tensor_scalar_t onto_calculate_diversity(
	const struct onto_kernel *population,
	int population_size);

/*
 * Find the best (highest fitness) kernel in a population
 */
extern int onto_find_best(
	const struct onto_kernel *population,
	int population_size);

/*
 * Print kernel information (for debugging)
 */
extern void onto_print_kernel(const struct onto_kernel *kernel);

/*
 * Print generation statistics (for debugging)
 */
extern void onto_print_stats(const struct onto_gen_stats *stats);

#endif /* _KERN_ONTOGENESIS_H_ */
