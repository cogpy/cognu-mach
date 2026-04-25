/*
 * GNU Mach Ontogenesis - Self-Generating Kernel System
 * Copyright (c) 2024-2025 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * Cherry-picked from ggnumlcash.cpp ontogenesis commits:
 *   7241b86eb3 - Implement ontogenesis: self-generating kernel system
 *   bba691e5cb - Add comprehensive test suite for ontogenesis
 *   967eaab846 - Update README with ontogenesis documentation
 *   ccf7dd145a - Merge pull request #18
 *
 * Ported from C++ (with STL, floats) to kernel C (fixed-point, static arrays).
 * Implements the behavioral contract from ONTOGENESIS.md spec.
 */

#include <kern/ontogenesis.h>
#include <kern/printf.h>
#include <kern/kalloc.h>
#include <string.h>

/*
 * Simple linear congruential PRNG (kernel-safe, no FPU)
 * Period: 2^32, constants from Numerical Recipes
 */
static unsigned int onto_rng_state = 12345;

static unsigned int
onto_rand(void)
{
	onto_rng_state = onto_rng_state * 1664525u + 1013904223u;
	return onto_rng_state;
}

/*
 * Random fixed-point value in [0, ONTO_FIXED_ONE]
 */
static tensor_scalar_t
onto_rand_fixed(void)
{
	return (tensor_scalar_t)(onto_rand() % (unsigned int)ONTO_FIXED_ONE);
}

/*
 * Random fixed-point value in [-ONTO_FIXED_ONE, ONTO_FIXED_ONE]
 */
static tensor_scalar_t
onto_rand_signed(void)
{
	return (tensor_scalar_t)(onto_rand() % (unsigned int)(2 * ONTO_FIXED_ONE))
		- ONTO_FIXED_ONE;
}

/*
 * Fixed-point absolute value
 */
static tensor_scalar_t
onto_abs(tensor_scalar_t x)
{
	return (x < 0) ? -x : x;
}

/*
 * Fixed-point minimum
 */
static tensor_scalar_t
onto_min(tensor_scalar_t a, tensor_scalar_t b)
{
	return (a < b) ? a : b;
}

/*
 * Fixed-point maximum
 */
static tensor_scalar_t
onto_max(tensor_scalar_t a, tensor_scalar_t b)
{
	return (a > b) ? a : b;
}

/*
 * Integer minimum
 */
static int
onto_imin(int a, int b)
{
	return (a < b) ? a : b;
}

/*
 * Integer maximum
 */
static int
onto_imax(int a, int b)
{
	return (a > b) ? a : b;
}

/*
 * Fixed-point approximate square root (Newton's method, 8 iterations)
 */
static tensor_scalar_t
onto_sqrt(tensor_scalar_t x)
{
	tensor_scalar_t guess;
	int i;

	if (x <= 0)
		return 0;

	/* Initial guess: half of input */
	guess = x / 2;
	if (guess == 0)
		guess = 1;

	for (i = 0; i < 8; i++) {
		tensor_scalar_t next = (guess + ONTO_FIXED_DIV(x, guess)) / 2;
		if (next == guess)
			break;
		guess = next;
	}

	return guess;
}

/*
 * Kernel ID counter
 */
static int onto_id_counter = 0;

/*
 * Initialize the ontogenesis subsystem
 */
void
ontogenesis_init(void)
{
	onto_rng_state = 12345;
	onto_id_counter = 0;
}

/*
 * Generate a unique kernel ID
 */
void
onto_generate_id(char *buf, int buflen)
{
	int cnt = onto_id_counter++;
	int i;

	if (buflen < ONTO_ID_LEN)
		return;

	/* Format: K00000000-XXXX */
	buf[0] = 'K';
	for (i = 8; i >= 1; i--) {
		buf[i] = '0' + (cnt % 10);
		cnt /= 10;
	}
	buf[9] = '-';

	/* Append a hex suffix from RNG for uniqueness */
	{
		unsigned int r = onto_rand();
		static const char hex[] = "0123456789abcdef";
		buf[10] = hex[(r >> 12) & 0xf];
		buf[11] = hex[(r >>  8) & 0xf];
		buf[12] = hex[(r >>  4) & 0xf];
		buf[13] = hex[(r >>  0) & 0xf];
		buf[14] = '\0';
	}
}

/*
 * Initialize an ontogenetic kernel from coefficients
 */
kern_return_t
onto_kernel_init(struct onto_kernel *kernel,
		 const tensor_scalar_t *coefficients,
		 uint16_t num_coefficients,
		 int order)
{
	int i;

	if (!kernel || !coefficients || num_coefficients == 0)
		return KERN_INVALID_ARGUMENT;
	if (num_coefficients > ONTO_MAX_COEFFICIENTS)
		num_coefficients = ONTO_MAX_COEFFICIENTS;

	memset(kernel, 0, sizeof(*kernel));

	/* Set coefficients */
	kernel->order = order;
	kernel->num_coefficients = num_coefficients;
	for (i = 0; i < num_coefficients; i++)
		kernel->coefficients[i] = coefficients[i];

	/* Initialize genome */
	onto_generate_id(kernel->genome.id, ONTO_ID_LEN);
	kernel->genome.generation = 0;
	kernel->genome.lineage_count = 0;
	kernel->genome.age = 0;
	kernel->genome.fitness = 0;

	/* Create coefficient gene from initial coefficients */
	kernel->genome.genes[0].type = ONTO_GENE_COEFFICIENT;
	kernel->genome.genes[0].is_mutable = TRUE;
	kernel->genome.genes[0].num_values = num_coefficients;
	for (i = 0; i < num_coefficients; i++)
		kernel->genome.genes[0].values[i] = coefficients[i];

	/* Create operator gene with default weights: chain=1.0, product=0.5, quotient=0.25 */
	kernel->genome.genes[1].type = ONTO_GENE_OPERATOR;
	kernel->genome.genes[1].is_mutable = TRUE;
	kernel->genome.genes[1].num_values = 3;
	kernel->genome.genes[1].values[0] = ONTO_FIXED_ONE;
	kernel->genome.genes[1].values[1] = ONTO_FIXED_HALF;
	kernel->genome.genes[1].values[2] = ONTO_FIXED_ONE / 4;

	kernel->genome.gene_count = 2;

	/* Initialize state: embryonic */
	kernel->state.stage = ONTO_STAGE_EMBRYONIC;
	kernel->state.maturity = 0;
	kernel->state.development_cycles = 0;

	kernel->history_count = 0;

	simple_lock_init(&kernel->lock);

	return KERN_SUCCESS;
}

/*
 * Initialize default evolution configuration
 * (per ONTOGENESIS.md spec default values)
 */
void
onto_config_default(struct onto_config *config)
{
	if (!config)
		return;

	memset(config, 0, sizeof(*config));

	/* Evolution defaults from spec */
	config->evolution.population_size = 20;
	config->evolution.mutation_rate = ONTO_FIXED_ONE / 10;       /* 0.1 */
	config->evolution.crossover_rate = ONTO_FIXED_ONE * 7 / 10;  /* 0.7 */
	config->evolution.elitism_rate = ONTO_FIXED_ONE / 5;         /* 0.2 */
	config->evolution.max_generations = 100;
	config->evolution.fitness_threshold = ONTO_FIXED_ONE * 9 / 10; /* 0.9 */
	config->evolution.diversity_pressure = ONTO_FIXED_ONE / 10;  /* 0.1 */

	/* Development schedule defaults from spec */
	config->development.embryonic_duration = 2;
	config->development.juvenile_duration = 5;
	config->development.mature_duration = 10;
	config->development.maturity_threshold = ONTO_FIXED_ONE * 4 / 5; /* 0.8 */

	config->seed_count = 0;
}

/*
 * Calculate total grip score (weighted combination per ONTOGENESIS.md spec)
 * grip = contact*0.4 + stability*0.2 + efficiency*0.2 + novelty*0.1 + symmetry*0.1
 */
tensor_scalar_t
onto_grip_total(const struct onto_grip *grip)
{
	tensor_scalar_t total;

	total = ONTO_FIXED_MUL(grip->contact,    ONTO_FIXED_ONE * 4 / 10)
	      + ONTO_FIXED_MUL(grip->stability,  ONTO_FIXED_ONE * 2 / 10)
	      + ONTO_FIXED_MUL(grip->efficiency, ONTO_FIXED_ONE * 2 / 10)
	      + ONTO_FIXED_MUL(grip->novelty,    ONTO_FIXED_ONE / 10)
	      + ONTO_FIXED_MUL(grip->symmetry,   ONTO_FIXED_ONE / 10);

	return total;
}

/*
 * Evaluate grip metrics for a kernel
 */
struct onto_grip
onto_evaluate_grip(const struct onto_kernel *kernel)
{
	struct onto_grip metrics;
	tensor_scalar_t sum, variation;
	int non_zero, i;

	memset(&metrics, 0, sizeof(metrics));

	if (!kernel || kernel->num_coefficients == 0)
		return metrics;

	/* Contact: coefficient magnitude distribution */
	sum = 0;
	for (i = 0; i < kernel->num_coefficients; i++)
		sum += onto_abs(kernel->coefficients[i]);
	metrics.contact = onto_min(ONTO_FIXED_ONE,
		ONTO_FIXED_DIV(sum, TENSOR_TO_FIXED(kernel->num_coefficients + 1)));

	/* Coverage: ratio of non-zero coefficients */
	non_zero = 0;
	for (i = 0; i < kernel->num_coefficients; i++) {
		if (onto_abs(kernel->coefficients[i]) > 1)
			non_zero++;
	}
	metrics.coverage = ONTO_FIXED_DIV(
		TENSOR_TO_FIXED(non_zero),
		TENSOR_TO_FIXED(onto_imax(1, kernel->num_coefficients)));

	/* Efficiency: prefer simpler (lower order) methods */
	metrics.efficiency = ONTO_FIXED_DIV(
		ONTO_FIXED_ONE,
		ONTO_FIXED_ONE + ONTO_FIXED_MUL(TENSOR_TO_FIXED(kernel->order),
						 ONTO_FIXED_TENTH));

	/* Stability: coefficient smoothness (inverse of variation) */
	variation = 0;
	for (i = 1; i < kernel->num_coefficients; i++) {
		tensor_scalar_t diff = kernel->coefficients[i]
				     - kernel->coefficients[i - 1];
		variation += ONTO_FIXED_MUL(diff, diff);
	}
	metrics.stability = ONTO_FIXED_DIV(ONTO_FIXED_ONE,
		ONTO_FIXED_ONE + variation);

	/* Novelty and symmetry: default 0.5 (updated during evolution) */
	metrics.novelty = ONTO_FIXED_HALF;
	metrics.symmetry = ONTO_FIXED_HALF;

	return metrics;
}

/*
 * Self-generate: Create offspring through recursive self-composition
 * Applies chain rule: (f∘f)' = f'(f(x)) · f'(x)
 */
kern_return_t
onto_self_generate(const struct onto_kernel *parent,
		   struct onto_kernel *offspring)
{
	int i;
	struct onto_event *evt;

	if (!parent || !offspring)
		return KERN_INVALID_ARGUMENT;

	/* Copy parent to offspring */
	memcpy(offspring, parent, sizeof(*offspring));

	/* Update genome */
	onto_generate_id(offspring->genome.id, ONTO_ID_LEN);
	offspring->genome.generation = parent->genome.generation + 1;

	/* Add parent to lineage */
	if (offspring->genome.lineage_count < ONTO_MAX_LINEAGE) {
		memcpy(offspring->genome.lineage[offspring->genome.lineage_count],
		       parent->genome.id, ONTO_ID_LEN);
		offspring->genome.lineage_count++;
	}
	offspring->genome.age = 0;

	/* Apply chain rule to coefficients: derivative of composition
	 * For B-series: (f∘f)(x) has modified coefficients */
	for (i = 0; i < offspring->num_coefficients; i++) {
		tensor_scalar_t derivative;
		if (i > 0)
			derivative = ONTO_FIXED_MUL(parent->coefficients[i - 1],
						    parent->coefficients[i]);
		else
			derivative = parent->coefficients[i];

		offspring->coefficients[i] = parent->coefficients[i]
			+ ONTO_FIXED_MUL(ONTO_FIXED_TENTH, derivative);
	}

	/* Update genes */
	if (offspring->genome.gene_count > 0) {
		for (i = 0; i < offspring->num_coefficients
		     && i < ONTO_MAX_GENE_VALUES; i++)
			offspring->genome.genes[0].values[i] =
				offspring->coefficients[i];
	}

	/* Reset state to embryonic */
	offspring->state.stage = ONTO_STAGE_EMBRYONIC;
	offspring->state.maturity = 0;
	offspring->state.development_cycles = 0;

	/* Record development event */
	offspring->history_count = 0;
	if (offspring->history_count < ONTO_MAX_HISTORY) {
		evt = &offspring->history[offspring->history_count];
		evt->iteration = 0;
		evt->stage = ONTO_STAGE_EMBRYONIC;
		evt->grip = 0;
		strncpy(evt->description, "Self-generated from parent",
			ONTO_DESC_LEN - 1);
		evt->description[ONTO_DESC_LEN - 1] = '\0';
		offspring->history_count++;
	}

	simple_lock_init(&offspring->lock);

	return KERN_SUCCESS;
}

/*
 * Self-optimize: Improve grip through iterative gradient ascent
 */
kern_return_t
onto_self_optimize(struct onto_kernel *kernel, int iterations)
{
	int iter, i;
	struct onto_kernel *temp;

	if (!kernel || iterations <= 0)
		return KERN_INVALID_ARGUMENT;

	/* Allocate temp kernel via kalloc (too large for kernel stack) */
	temp = (struct onto_kernel *)kalloc(sizeof(struct onto_kernel));
	if (!temp)
		return KERN_RESOURCE_SHORTAGE;

	for (iter = 0; iter < iterations; iter++) {
		struct onto_grip grip;
		tensor_scalar_t current_grip;
		tensor_scalar_t gradient[ONTO_MAX_COEFFICIENTS];
		tensor_scalar_t epsilon = ONTO_FIXED_ONE / 100; /* 0.01 */
		tensor_scalar_t learning_rate = ONTO_FIXED_ONE / 100;
		struct onto_event *evt;

		/* Evaluate current grip */
		grip = onto_evaluate_grip(kernel);
		current_grip = onto_grip_total(&grip);

		/* Compute gradient by finite differences */
		for (i = 0; i < kernel->num_coefficients; i++) {
			struct onto_grip perturbed_grip;
			tensor_scalar_t perturbed_total;
			tensor_scalar_t saved;

			memcpy(temp, kernel, sizeof(*temp));
			saved = temp->coefficients[i];
			temp->coefficients[i] += epsilon;

			if (temp->genome.gene_count > 0
			    && i < ONTO_MAX_GENE_VALUES)
				temp->genome.genes[0].values[i] =
					temp->coefficients[i];

			perturbed_grip = onto_evaluate_grip(temp);
			perturbed_total = onto_grip_total(&perturbed_grip);

			gradient[i] = ONTO_FIXED_DIV(
				perturbed_total - current_grip, epsilon);
		}

		/* Gradient ascent step */
		for (i = 0; i < kernel->num_coefficients; i++) {
			kernel->coefficients[i] += ONTO_FIXED_MUL(
				learning_rate, gradient[i]);
		}

		/* Update genes */
		if (kernel->genome.gene_count > 0) {
			for (i = 0; i < kernel->num_coefficients
			     && i < ONTO_MAX_GENE_VALUES; i++)
				kernel->genome.genes[0].values[i] =
					kernel->coefficients[i];
		}

		/* Update maturity (increase by 0.1 each iter, cap at 1.0) */
		kernel->state.maturity = onto_min(ONTO_FIXED_ONE,
			kernel->state.maturity + ONTO_FIXED_TENTH);
		kernel->state.development_cycles++;

		/* Record event */
		if (kernel->history_count < ONTO_MAX_HISTORY) {
			evt = &kernel->history[kernel->history_count];
			evt->iteration = iter;
			evt->grip = current_grip;
			evt->stage = kernel->state.stage;
			strncpy(evt->description, "Optimization iteration",
				ONTO_DESC_LEN - 1);
			evt->description[ONTO_DESC_LEN - 1] = '\0';
			kernel->history_count++;
		}
	}

	kfree((vm_offset_t)temp, sizeof(struct onto_kernel));
	return KERN_SUCCESS;
}

/*
 * Crossover two genomes (single-point crossover on coefficient arrays)
 */
kern_return_t
onto_crossover(const struct onto_genome *g1,
	       const struct onto_genome *g2,
	       struct onto_genome *offspring)
{
	int i, min_genes, min_len;
	unsigned int crossover_point;

	if (!g1 || !g2 || !offspring)
		return KERN_INVALID_ARGUMENT;

	memset(offspring, 0, sizeof(*offspring));

	onto_generate_id(offspring->id, ONTO_ID_LEN);
	offspring->generation = onto_imax(g1->generation, g2->generation) + 1;

	/* Record lineage */
	offspring->lineage_count = 0;
	if (offspring->lineage_count < ONTO_MAX_LINEAGE) {
		memcpy(offspring->lineage[offspring->lineage_count],
		       g1->id, ONTO_ID_LEN);
		offspring->lineage_count++;
	}
	if (offspring->lineage_count < ONTO_MAX_LINEAGE) {
		memcpy(offspring->lineage[offspring->lineage_count],
		       g2->id, ONTO_ID_LEN);
		offspring->lineage_count++;
	}
	offspring->age = 0;

	/* Crossover genes */
	min_genes = onto_imin(g1->gene_count, g2->gene_count);
	offspring->gene_count = min_genes;

	for (i = 0; i < min_genes; i++) {
		if (g1->genes[i].is_mutable && g2->genes[i].is_mutable) {
			min_len = onto_imin(g1->genes[i].num_values,
					    g2->genes[i].num_values);
			if (min_len > 0) {
				int j;
				crossover_point = onto_rand() % min_len;

				offspring->genes[i] = g1->genes[i];
				offspring->genes[i].num_values = min_len;
				for (j = crossover_point; j < min_len; j++)
					offspring->genes[i].values[j] =
						g2->genes[i].values[j];
			} else {
				offspring->genes[i] = g1->genes[i];
			}
		} else {
			/* Immutable genes: randomly choose parent */
			offspring->genes[i] = (onto_rand() % 2 == 0)
				? g1->genes[i] : g2->genes[i];
		}
	}

	return KERN_SUCCESS;
}

/*
 * Mutate a genome (random perturbation of mutable gene values)
 */
void
onto_mutate(struct onto_genome *genome, tensor_scalar_t mutation_rate)
{
	int i, j;

	if (!genome)
		return;

	for (i = 0; i < genome->gene_count; i++) {
		if (!genome->genes[i].is_mutable)
			continue;
		for (j = 0; j < genome->genes[i].num_values; j++) {
			/* Mutate with probability mutation_rate */
			if (onto_rand_fixed() < mutation_rate) {
				/* ±10% perturbation (±0.2 in fixed-point range) */
				tensor_scalar_t perturbation =
					onto_rand_signed() / 5;
				genome->genes[i].values[j] += perturbation;
			}
		}
	}
}

/*
 * Self-reproduce: Combine two kernels to create offspring
 */
kern_return_t
onto_self_reproduce(const struct onto_kernel *parent1,
		    const struct onto_kernel *parent2,
		    onto_reproduction_t method,
		    struct onto_kernel *offspring)
{
	int i, min_size;

	if (!parent1 || !parent2 || !offspring)
		return KERN_INVALID_ARGUMENT;

	memset(offspring, 0, sizeof(*offspring));

	switch (method) {
	case ONTO_REPRO_CROSSOVER: {
		unsigned int point;

		onto_crossover(&parent1->genome, &parent2->genome,
			       &offspring->genome);

		/* Combine coefficients through crossover */
		min_size = onto_imin(parent1->num_coefficients,
				     parent2->num_coefficients);
		offspring->num_coefficients = min_size;

		point = (min_size > 0) ? (onto_rand() % min_size) : 0;
		for (i = 0; i < min_size; i++) {
			offspring->coefficients[i] = ((unsigned int)i < point)
				? parent1->coefficients[i]
				: parent2->coefficients[i];
		}

		/* Sync gene values to match coefficients */
		if (offspring->genome.gene_count > 0) {
			for (i = 0; i < min_size
			     && i < ONTO_MAX_GENE_VALUES; i++)
				offspring->genome.genes[0].values[i] =
					offspring->coefficients[i];
			offspring->genome.genes[0].num_values = min_size;
		}
		break;
	}

	case ONTO_REPRO_MUTATION:
		memcpy(offspring, parent1, sizeof(*offspring));
		onto_generate_id(offspring->genome.id, ONTO_ID_LEN);
		offspring->genome.generation = parent1->genome.generation + 1;

		if (offspring->genome.lineage_count < ONTO_MAX_LINEAGE) {
			memcpy(offspring->genome.lineage[
				offspring->genome.lineage_count],
			       parent1->genome.id, ONTO_ID_LEN);
			offspring->genome.lineage_count++;
		}

		onto_mutate(&offspring->genome, ONTO_FIXED_ONE / 5);

		/* Sync mutated gene values to coefficients */
		if (offspring->genome.gene_count > 0) {
			for (i = 0; i < offspring->num_coefficients
			     && i < ONTO_MAX_GENE_VALUES; i++)
				offspring->coefficients[i] =
					offspring->genome.genes[0].values[i];
		}
		break;

	case ONTO_REPRO_CLONING:
		memcpy(offspring, parent1, sizeof(*offspring));
		onto_generate_id(offspring->genome.id, ONTO_ID_LEN);

		if (offspring->genome.lineage_count < ONTO_MAX_LINEAGE) {
			memcpy(offspring->genome.lineage[
				offspring->genome.lineage_count],
			       parent1->genome.id, ONTO_ID_LEN);
			offspring->genome.lineage_count++;
		}
		break;
	}

	offspring->order = parent1->order;
	offspring->genome.age = 0;
	offspring->state.stage = ONTO_STAGE_EMBRYONIC;
	offspring->state.maturity = 0;
	offspring->state.development_cycles = 0;
	offspring->history_count = 0;

	simple_lock_init(&offspring->lock);

	return KERN_SUCCESS;
}

/*
 * Calculate genetic distance between two kernels
 * Returns fixed-point RMS distance
 */
tensor_scalar_t
onto_genetic_distance(const struct onto_kernel *k1,
		      const struct onto_kernel *k2)
{
	tensor_scalar_t distance = 0;
	int count, min_coeff, i;

	if (!k1 || !k2)
		return 0;

	min_coeff = onto_imin(k1->num_coefficients, k2->num_coefficients);
	count = 0;

	for (i = 0; i < min_coeff; i++) {
		tensor_scalar_t diff = k1->coefficients[i]
				     - k2->coefficients[i];
		distance += ONTO_FIXED_MUL(diff, diff);
		count++;
	}

	if (count == 0)
		return 0;

	return onto_sqrt(ONTO_FIXED_DIV(distance, TENSOR_TO_FIXED(count)));
}

/*
 * Calculate fitness including population diversity
 */
tensor_scalar_t
onto_calculate_fitness(const struct onto_kernel *kernel,
		       const struct onto_kernel *population,
		       int population_size)
{
	struct onto_grip grip;
	tensor_scalar_t grip_fitness, total_distance, novelty;
	int count, i;

	if (!kernel)
		return 0;

	grip = onto_evaluate_grip(kernel);
	grip_fitness = onto_grip_total(&grip);

	/* Calculate novelty (average distance to population) */
	total_distance = 0;
	count = 0;

	if (population) {
		for (i = 0; i < population_size; i++) {
			/* Skip self (compare IDs) */
			if (strncmp(population[i].genome.id,
				    kernel->genome.id, ONTO_ID_LEN) == 0)
				continue;
			total_distance += onto_genetic_distance(
				kernel, &population[i]);
			count++;
		}
	}

	novelty = (count > 0)
		? ONTO_FIXED_DIV(total_distance, TENSOR_TO_FIXED(count))
		: ONTO_FIXED_HALF;
	novelty = onto_min(ONTO_FIXED_ONE, novelty);

	/* Combined fitness: grip*0.9 + novelty*0.1 */
	return ONTO_FIXED_MUL(grip_fitness, ONTO_FIXED_ONE * 9 / 10)
	     + ONTO_FIXED_MUL(novelty, ONTO_FIXED_ONE / 10);
}

/*
 * Update fitness for entire population
 */
static void
onto_update_population_fitness(struct onto_kernel *population,
			       int population_size)
{
	int i;

	for (i = 0; i < population_size; i++)
		population[i].genome.fitness = onto_calculate_fitness(
			&population[i], population, population_size);
}

/*
 * Tournament selection: select 'count' individuals via tournament
 */
static int
onto_tournament_select(const struct onto_kernel *population,
		       int population_size,
		       struct onto_kernel *selected,
		       int count,
		       int tournament_size)
{
	int i, j, sel = 0;

	for (i = 0; i < count && sel < count; i++) {
		const struct onto_kernel *best = NULL;
		tensor_scalar_t best_fitness = -1;

		for (j = 0; j < tournament_size; j++) {
			int idx = onto_rand() % population_size;
			if (population[idx].genome.fitness > best_fitness) {
				best_fitness = population[idx].genome.fitness;
				best = &population[idx];
			}
		}

		if (best) {
			memcpy(&selected[sel], best, sizeof(*best));
			sel++;
		}
	}

	return sel;
}

/*
 * Simple insertion sort for population by fitness (descending)
 */
static void
onto_sort_by_fitness(struct onto_kernel *population, int size)
{
	int i, j;
	struct onto_kernel *key;

	if (size <= 1)
		return;

	/* Allocate temp key via kalloc (too large for kernel stack) */
	key = (struct onto_kernel *)kalloc(sizeof(struct onto_kernel));
	if (!key)
		return;

	for (i = 1; i < size; i++) {
		memcpy(key, &population[i], sizeof(*key));
		j = i - 1;
		while (j >= 0
		       && population[j].genome.fitness < key->genome.fitness) {
			memcpy(&population[j + 1], &population[j],
			       sizeof(population[j]));
			j--;
		}
		memcpy(&population[j + 1], key, sizeof(*key));
	}

	kfree((vm_offset_t)key, sizeof(struct onto_kernel));
}

/*
 * Evolve a single generation
 */
void
onto_evolve_generation(struct onto_kernel *population,
		       int *population_size,
		       const struct onto_evolution_config *config)
{
	struct onto_kernel *new_pop;
	struct onto_kernel *parents;
	int elite_count, new_size, i;

	if (!population || !population_size || !config
	    || *population_size <= 0)
		return;

	/* Allocate working arrays via kalloc (too large for kernel stack) */
	new_pop = (struct onto_kernel *)kalloc(
		ONTO_MAX_POPULATION * sizeof(struct onto_kernel));
	parents = (struct onto_kernel *)kalloc(
		2 * sizeof(struct onto_kernel));
	if (!new_pop || !parents) {
		if (new_pop)
			kfree((vm_offset_t)new_pop,
			      ONTO_MAX_POPULATION * sizeof(struct onto_kernel));
		if (parents)
			kfree((vm_offset_t)parents,
			      2 * sizeof(struct onto_kernel));
		return;
	}

	/* Update fitness */
	onto_update_population_fitness(population, *population_size);

	/* Sort by fitness (descending) */
	onto_sort_by_fitness(population, *population_size);

	/* Keep elite individuals (fixed-point multiply: pop_size * elitism_rate) */
	elite_count = (int)(((long)*population_size
		* (long)config->elitism_rate) >> TENSOR_FIXED_POINT_BITS);
	if (elite_count < 1 && *population_size > 0)
		elite_count = 1;
	if (elite_count > ONTO_MAX_POPULATION)
		elite_count = ONTO_MAX_POPULATION;

	new_size = 0;
	for (i = 0; i < elite_count && i < *population_size; i++) {
		memcpy(&new_pop[new_size], &population[i],
		       sizeof(population[i]));
		new_size++;
	}

	/* Generate offspring until population is full */
	while (new_size < config->population_size
	       && new_size < ONTO_MAX_POPULATION) {
		int nsel;

		nsel = onto_tournament_select(population, *population_size,
					      parents, 2, 3);
		if (nsel >= 2) {
			struct onto_kernel *child;

			child = (struct onto_kernel *)kalloc(
				sizeof(struct onto_kernel));
			if (!child)
				break;

			if (onto_rand_fixed() < config->crossover_rate)
				onto_self_reproduce(&parents[0], &parents[1],
						    ONTO_REPRO_CROSSOVER,
						    child);
			else
				onto_self_reproduce(&parents[0], &parents[1],
						    ONTO_REPRO_CLONING,
						    child);

			/* Apply mutation */
			if (onto_rand_fixed() < config->mutation_rate) {
				onto_mutate(&child->genome,
					    config->mutation_rate);
				/* Sync mutated gene values back to coefficients */
				if (child->genome.gene_count > 0) {
					int k;
					for (k = 0; k < child->num_coefficients
					     && k < ONTO_MAX_GENE_VALUES; k++)
						child->coefficients[k] =
							child->genome.genes[0].values[k];
				}
			}

			memcpy(&new_pop[new_size], child, sizeof(*child));
			new_size++;
			kfree((vm_offset_t)child,
			      sizeof(struct onto_kernel));
		}
	}

	/* Replace population */
	for (i = 0; i < new_size; i++)
		memcpy(&population[i], &new_pop[i], sizeof(new_pop[i]));
	*population_size = new_size;

	kfree((vm_offset_t)new_pop,
	      ONTO_MAX_POPULATION * sizeof(struct onto_kernel));
	kfree((vm_offset_t)parents,
	      2 * sizeof(struct onto_kernel));
}

/*
 * Update development stages based on age and maturity
 */
void
onto_update_stages(struct onto_kernel *population,
		   int population_size,
		   const struct onto_dev_schedule *schedule)
{
	int i;

	if (!population || !schedule)
		return;

	for (i = 0; i < population_size; i++) {
		struct onto_kernel *k = &population[i];
		k->genome.age++;

		if (k->genome.age < schedule->embryonic_duration) {
			k->state.stage = ONTO_STAGE_EMBRYONIC;
		} else if (k->genome.age < schedule->embryonic_duration
			   + schedule->juvenile_duration) {
			k->state.stage = ONTO_STAGE_JUVENILE;
		} else if (k->genome.age >= schedule->embryonic_duration
			   + schedule->juvenile_duration
			   + schedule->mature_duration) {
			k->state.stage = ONTO_STAGE_SENESCENT;
		} else if (k->state.maturity >= schedule->maturity_threshold) {
			k->state.stage = ONTO_STAGE_MATURE;
		}
	}
}

/*
 * Calculate population diversity (average genetic distance)
 */
tensor_scalar_t
onto_calculate_diversity(const struct onto_kernel *population,
			 int population_size)
{
	tensor_scalar_t total_distance = 0;
	int count = 0;
	int i, j;

	if (!population || population_size < 2)
		return 0;

	for (i = 0; i < population_size; i++) {
		for (j = i + 1; j < population_size; j++) {
			total_distance += onto_genetic_distance(
				&population[i], &population[j]);
			count++;
		}
	}

	return (count > 0)
		? ONTO_FIXED_DIV(total_distance, TENSOR_TO_FIXED(count))
		: 0;
}

/*
 * Find the best (highest fitness) kernel in a population
 * Returns index, or -1 if population is empty
 */
int
onto_find_best(const struct onto_kernel *population, int population_size)
{
	int best_idx = -1;
	tensor_scalar_t best_fitness = -1;
	int i;

	if (!population)
		return -1;

	for (i = 0; i < population_size; i++) {
		if (population[i].genome.fitness > best_fitness) {
			best_fitness = population[i].genome.fitness;
			best_idx = i;
		}
	}

	return best_idx;
}

/*
 * Generate initial random population from seeds
 */
static int
onto_generate_initial_population(struct onto_kernel *population,
				 int target_size,
				 int kernel_order,
				 const struct onto_kernel *seeds,
				 int seed_count)
{
	int pop_size = 0;
	int i;

	if (!population)
		return 0;

	/* Add seed kernels */
	for (i = 0; i < seed_count && pop_size < target_size
	     && pop_size < ONTO_MAX_POPULATION; i++) {
		memcpy(&population[pop_size], &seeds[i], sizeof(seeds[i]));
		pop_size++;
	}

	/* Generate random kernels to fill population */
	while (pop_size < target_size && pop_size < ONTO_MAX_POPULATION) {
		tensor_scalar_t coeffs[ONTO_MAX_COEFFICIENTS];
		int j;

		for (j = 0; j < kernel_order && j < ONTO_MAX_COEFFICIENTS; j++)
			coeffs[j] = onto_rand_signed();

		onto_kernel_init(&population[pop_size], coeffs,
				 kernel_order, kernel_order);
		pop_size++;
	}

	return pop_size;
}

/*
 * Run complete ontogenesis evolution
 */
kern_return_t
onto_run_evolution(const struct onto_config *config,
		   struct onto_gen_stats *stats,
		   int *num_generations)
{
	struct onto_kernel *population;
	int pop_size, gen, best_idx;
	tensor_scalar_t sum, min_fit;
	int i;

	if (!config || !stats || !num_generations)
		return KERN_INVALID_ARGUMENT;

	*num_generations = 0;

	/* Allocate population via kalloc (too large for kernel stack) */
	population = (struct onto_kernel *)kalloc(
		ONTO_MAX_POPULATION * sizeof(struct onto_kernel));
	if (!population)
		return KERN_RESOURCE_SHORTAGE;

	/* Generate initial population */
	pop_size = onto_generate_initial_population(
		population,
		onto_imin(config->evolution.population_size, ONTO_MAX_POPULATION),
		4, /* default kernel order */
		config->seed_kernels,
		config->seed_count);

	if (pop_size == 0) {
		kfree((vm_offset_t)population,
		      ONTO_MAX_POPULATION * sizeof(struct onto_kernel));
		return KERN_FAILURE;
	}

	for (gen = 0; gen < config->evolution.max_generations; gen++) {
		/* Evolve generation */
		onto_evolve_generation(population, &pop_size,
				       &config->evolution);
		onto_update_stages(population, pop_size,
				   &config->development);

		/* Collect statistics */
		stats[gen].generation = gen;
		stats[gen].population_size = pop_size;

		onto_update_population_fitness(population, pop_size);
		best_idx = onto_find_best(population, pop_size);

		stats[gen].best_fitness = (best_idx >= 0)
			? population[best_idx].genome.fitness : 0;

		sum = 0;
		min_fit = ONTO_FIXED_ONE;
		for (i = 0; i < pop_size; i++) {
			sum += population[i].genome.fitness;
			if (population[i].genome.fitness < min_fit)
				min_fit = population[i].genome.fitness;
		}
		stats[gen].average_fitness = (pop_size > 0)
			? ONTO_FIXED_DIV(sum, TENSOR_TO_FIXED(pop_size))
			: 0;
		stats[gen].worst_fitness = min_fit;
		stats[gen].diversity = onto_calculate_diversity(
			population, pop_size);

		(*num_generations)++;

		/* Check for convergence */
		if (best_idx >= 0
		    && population[best_idx].genome.fitness
		       >= config->evolution.fitness_threshold)
			break;
	}

	kfree((vm_offset_t)population,
	      ONTO_MAX_POPULATION * sizeof(struct onto_kernel));

	return KERN_SUCCESS;
}

/*
 * Print kernel information (debugging)
 */
void
onto_print_kernel(const struct onto_kernel *kernel)
{
	const char *stage_names[] = {
		"Embryonic", "Juvenile", "Mature", "Senescent"
	};

	if (!kernel)
		return;

	printf("Kernel ID: %s\n", kernel->genome.id);
	printf("  Generation: %d\n", kernel->genome.generation);
	printf("  Age: %d\n", kernel->genome.age);
	printf("  Fitness: %d (fixed-point)\n", kernel->genome.fitness);
	printf("  Stage: %s\n",
	       (kernel->state.stage <= ONTO_STAGE_SENESCENT)
	       ? stage_names[kernel->state.stage] : "Unknown");
	printf("  Order: %d, Coefficients: %d\n",
	       kernel->order, kernel->num_coefficients);
	printf("  Maturity: %d/%d\n",
	       kernel->state.maturity, ONTO_FIXED_ONE);
}

/*
 * Print generation statistics (debugging)
 */
void
onto_print_stats(const struct onto_gen_stats *stats)
{
	if (!stats)
		return;

	printf("Gen %d: best=%d avg=%d worst=%d div=%d pop=%d\n",
	       stats->generation,
	       stats->best_fitness,
	       stats->average_fitness,
	       stats->worst_fitness,
	       stats->diversity,
	       stats->population_size);
}
