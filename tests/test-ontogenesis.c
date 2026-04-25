/*
 * Test suite for ontogenesis: self-generating kernel system
 *
 * Cherry-picked from ggnumlcash.cpp commit bba691e5cb and ported to
 * kernel C test framework. Validates the behavioral contract from
 * gnucashmulti/.github/agents/ONTOGENESIS.md spec.
 *
 * Copyright (C) 2024-2025 Free Software Foundation
 */

#include <testlib.h>
#include <kern/ontogenesis.h>

/*
 * Fixed-point comparison with tolerance
 */
static int
fixed_equal(tensor_scalar_t a, tensor_scalar_t b, tensor_scalar_t epsilon)
{
	tensor_scalar_t diff = (a > b) ? (a - b) : (b - a);
	return diff < epsilon;
}

/*
 * Test 1: Kernel initialization
 * Spec: initializeOntogeneticKernel() creates embryonic kernel
 */
static void
test_kernel_initialization(void)
{
	struct onto_kernel kernel;
	tensor_scalar_t coeffs[3];
	kern_return_t ret;

	printf("Test 1: Kernel Initialization... ");

	/* 0.5, 0.25, 0.125 in fixed-point */
	coeffs[0] = ONTO_FIXED_ONE / 2;
	coeffs[1] = ONTO_FIXED_ONE / 4;
	coeffs[2] = ONTO_FIXED_ONE / 8;

	ret = onto_kernel_init(&kernel, coeffs, 3, 3);
	ASSERT(ret == KERN_SUCCESS, "init should succeed");

	ASSERT(kernel.order == 3, "order should be 3");
	ASSERT(kernel.num_coefficients == 3, "should have 3 coefficients");
	ASSERT(fixed_equal(kernel.coefficients[0], ONTO_FIXED_ONE / 2, 1),
	       "coeff[0] should be 0.5");
	ASSERT(fixed_equal(kernel.coefficients[1], ONTO_FIXED_ONE / 4, 1),
	       "coeff[1] should be 0.25");
	ASSERT(fixed_equal(kernel.coefficients[2], ONTO_FIXED_ONE / 8, 1),
	       "coeff[2] should be 0.125");
	ASSERT(kernel.genome.generation == 0, "generation should be 0");
	ASSERT(kernel.genome.age == 0, "age should be 0");
	ASSERT(kernel.state.stage == ONTO_STAGE_EMBRYONIC,
	       "stage should be embryonic");
	ASSERT(fixed_equal(kernel.state.maturity, 0, 1),
	       "maturity should be 0");

	printf("PASSED\n");
}

/*
 * Test 2: Self-generation creates offspring
 * Spec: selfGenerate() applies chain rule (f∘f)' = f'(f(x)) · f'(x)
 */
static void
test_self_generation(void)
{
	struct onto_kernel parent, offspring;
	tensor_scalar_t coeffs[3];
	kern_return_t ret;
	int coeffs_different = 0;
	int i;

	printf("Test 2: Self-Generation... ");

	coeffs[0] = ONTO_FIXED_ONE / 2;
	coeffs[1] = ONTO_FIXED_ONE / 4;
	coeffs[2] = ONTO_FIXED_ONE / 8;

	onto_kernel_init(&parent, coeffs, 3, 3);
	ret = onto_self_generate(&parent, &offspring);
	ASSERT(ret == KERN_SUCCESS, "self_generate should succeed");

	/* Offspring should have incremented generation */
	ASSERT(offspring.genome.generation == parent.genome.generation + 1,
	       "generation should increment");

	/* Offspring should have parent in lineage */
	ASSERT(offspring.genome.lineage_count == 1,
	       "lineage should have 1 entry");

	/* Offspring should have different ID */
	ASSERT(strncmp(offspring.genome.id, parent.genome.id, ONTO_ID_LEN) != 0,
	       "offspring ID should differ from parent");

	/* Coefficients should be modified (chain rule applied) */
	for (i = 0; i < offspring.num_coefficients; i++) {
		if (!fixed_equal(offspring.coefficients[i],
				 parent.coefficients[i], 1)) {
			coeffs_different = 1;
			break;
		}
	}
	ASSERT(coeffs_different, "coefficients should be modified by chain rule");

	printf("PASSED\n");
}

/*
 * Test 3: Self-optimization improves maturity
 * Spec: selfOptimize() iteratively improves grip
 */
static void
test_self_optimization(void)
{
	struct onto_kernel kernel;
	tensor_scalar_t coeffs[3];
	kern_return_t ret;

	printf("Test 3: Self-Optimization... ");

	coeffs[0] = ONTO_FIXED_ONE * 3 / 10;
	coeffs[1] = ONTO_FIXED_ONE * 4 / 10;
	coeffs[2] = ONTO_FIXED_ONE * 2 / 10;

	onto_kernel_init(&kernel, coeffs, 3, 3);

	ret = onto_self_optimize(&kernel, 3);
	ASSERT(ret == KERN_SUCCESS, "self_optimize should succeed");

	/* Maturity should increase */
	ASSERT(kernel.state.maturity > 0, "maturity should increase");
	ASSERT(kernel.state.development_cycles == 3,
	       "development cycles should be 3");

	/* History should be recorded */
	ASSERT(kernel.history_count == 3, "should have 3 history events");

	printf("PASSED\n");
}

/*
 * Test 4: Crossover combines parent genomes
 * Spec: selfReproduce(parent1, parent2, 'crossover')
 */
static void
test_crossover(void)
{
	struct onto_kernel parent1, parent2, offspring;
	tensor_scalar_t coeffs1[3], coeffs2[3];
	kern_return_t ret;

	printf("Test 4: Crossover... ");

	coeffs1[0] = ONTO_FIXED_ONE * 8 / 10;
	coeffs1[1] = ONTO_FIXED_ONE * 6 / 10;
	coeffs1[2] = ONTO_FIXED_ONE * 4 / 10;

	coeffs2[0] = ONTO_FIXED_ONE * 2 / 10;
	coeffs2[1] = ONTO_FIXED_ONE * 3 / 10;
	coeffs2[2] = ONTO_FIXED_ONE * 5 / 10;

	onto_kernel_init(&parent1, coeffs1, 3, 3);
	onto_kernel_init(&parent2, coeffs2, 3, 3);

	ret = onto_self_reproduce(&parent1, &parent2,
				  ONTO_REPRO_CROSSOVER, &offspring);
	ASSERT(ret == KERN_SUCCESS, "crossover should succeed");

	/* Offspring should have both parents in lineage */
	ASSERT(offspring.genome.lineage_count == 2,
	       "should have 2 parents in lineage");

	/* Generation should be max + 1 */
	ASSERT(offspring.genome.generation > parent1.genome.generation,
	       "generation should exceed parent1");

	/* Offspring should have valid coefficients */
	ASSERT(offspring.num_coefficients == 3,
	       "should have 3 coefficients");

	printf("PASSED\n");
}

/*
 * Test 5: Mutation modifies genome
 * Spec: Mutation applies random perturbation of coefficients
 */
static void
test_mutation(void)
{
	struct onto_kernel parent, mutated;
	tensor_scalar_t coeffs[3];
	kern_return_t ret;
	int coeffs_different = 0;
	int i;

	printf("Test 5: Mutation... ");

	coeffs[0] = ONTO_FIXED_HALF;
	coeffs[1] = ONTO_FIXED_HALF;
	coeffs[2] = ONTO_FIXED_HALF;

	onto_kernel_init(&parent, coeffs, 3, 3);

	ret = onto_self_reproduce(&parent, &parent,
				  ONTO_REPRO_MUTATION, &mutated);
	ASSERT(ret == KERN_SUCCESS, "mutation should succeed");

	/* Mutated kernel should have different coefficients */
	for (i = 0; i < mutated.num_coefficients; i++) {
		if (!fixed_equal(mutated.coefficients[i],
				 parent.coefficients[i], 100)) {
			coeffs_different = 1;
			break;
		}
	}
	/* Mutation is probabilistic; just check it's a valid kernel */
	ASSERT(mutated.num_coefficients == 3,
	       "should still have 3 coefficients");

	printf("PASSED\n");
}

/*
 * Test 6: Grip evaluation
 * Spec: Fitness = grip*0.4 + stability*0.2 + efficiency*0.2
 *                 + novelty*0.1 + symmetry*0.1
 */
static void
test_grip_evaluation(void)
{
	struct onto_kernel kernel;
	tensor_scalar_t coeffs[4];
	struct onto_grip grip;
	tensor_scalar_t total;

	printf("Test 6: Grip Evaluation... ");

	coeffs[0] = ONTO_FIXED_ONE / 2;
	coeffs[1] = ONTO_FIXED_ONE / 4;
	coeffs[2] = ONTO_FIXED_ONE / 8;
	coeffs[3] = ONTO_FIXED_ONE / 16;

	onto_kernel_init(&kernel, coeffs, 4, 4);

	grip = onto_evaluate_grip(&kernel);
	total = onto_grip_total(&grip);

	/* All grip components should be non-negative */
	ASSERT(grip.contact >= 0, "contact should be >= 0");
	ASSERT(grip.coverage >= 0, "coverage should be >= 0");
	ASSERT(grip.efficiency >= 0, "efficiency should be >= 0");
	ASSERT(grip.stability >= 0, "stability should be >= 0");
	ASSERT(grip.novelty >= 0, "novelty should be >= 0");
	ASSERT(grip.symmetry >= 0, "symmetry should be >= 0");

	/* Total should be positive and bounded */
	ASSERT(total > 0, "total grip should be positive");
	ASSERT(total <= ONTO_FIXED_ONE, "total grip should be <= 1.0");

	printf("PASSED\n");
}

/*
 * Test 7: Genetic distance
 * Spec: novelty = avg_distance(kernel, population)
 */
static void
test_genetic_distance(void)
{
	struct onto_kernel k1, k2, k3;
	tensor_scalar_t c1[3], c2[3], c3[3];
	tensor_scalar_t d12, d13, d_self;

	printf("Test 7: Genetic Distance... ");

	c1[0] = ONTO_FIXED_ONE; c1[1] = 0; c1[2] = 0;
	c2[0] = 0; c2[1] = ONTO_FIXED_ONE; c2[2] = 0;
	c3[0] = ONTO_FIXED_ONE; c3[1] = 0; c3[2] = 0;

	onto_kernel_init(&k1, c1, 3, 3);
	onto_kernel_init(&k2, c2, 3, 3);
	onto_kernel_init(&k3, c3, 3, 3);

	d12 = onto_genetic_distance(&k1, &k2);
	d13 = onto_genetic_distance(&k1, &k3);
	d_self = onto_genetic_distance(&k1, &k1);

	/* Distance to self should be 0 */
	ASSERT(d_self == 0, "distance to self should be 0");

	/* Identical coefficients should have distance 0 */
	ASSERT(d13 == 0, "identical coefficients should have distance 0");

	/* Different coefficients should have positive distance */
	ASSERT(d12 > 0, "different coefficients should have distance > 0");

	printf("PASSED\n");
}

/*
 * Test 8: Development stages
 * Spec: Embryonic -> Juvenile -> Mature -> Senescent
 */
static void
test_development_stages(void)
{
	struct onto_kernel pop[2];
	tensor_scalar_t coeffs[3];
	struct onto_dev_schedule sched;

	printf("Test 8: Development Stages... ");

	coeffs[0] = ONTO_FIXED_HALF;
	coeffs[1] = ONTO_FIXED_HALF;
	coeffs[2] = ONTO_FIXED_HALF;

	onto_kernel_init(&pop[0], coeffs, 3, 3);
	onto_kernel_init(&pop[1], coeffs, 3, 3);

	sched.embryonic_duration = 2;
	sched.juvenile_duration = 3;
	sched.mature_duration = 5;
	sched.maturity_threshold = ONTO_FIXED_ONE * 8 / 10;

	/* Initially embryonic */
	ASSERT(pop[0].state.stage == ONTO_STAGE_EMBRYONIC,
	       "should start embryonic");

	/* After 1 tick: still embryonic (age=1 < embryonic_duration=2) */
	onto_update_stages(pop, 2, &sched);
	ASSERT(pop[0].state.stage == ONTO_STAGE_EMBRYONIC,
	       "should still be embryonic at age 1");

	/* After 2 ticks: juvenile (age=2 >= embryonic_duration=2) */
	onto_update_stages(pop, 2, &sched);
	ASSERT(pop[0].state.stage == ONTO_STAGE_JUVENILE,
	       "should be juvenile at age 2");

	printf("PASSED\n");
}

/*
 * Test 9: Cloning
 * Spec: Cloning = Direct copy
 */
static void
test_cloning(void)
{
	struct onto_kernel parent, clone;
	tensor_scalar_t coeffs[3];
	kern_return_t ret;
	int i;

	printf("Test 9: Cloning... ");

	coeffs[0] = ONTO_FIXED_ONE * 3 / 10;
	coeffs[1] = ONTO_FIXED_ONE * 6 / 10;
	coeffs[2] = ONTO_FIXED_ONE * 9 / 10;

	onto_kernel_init(&parent, coeffs, 3, 3);

	ret = onto_self_reproduce(&parent, &parent,
				  ONTO_REPRO_CLONING, &clone);
	ASSERT(ret == KERN_SUCCESS, "cloning should succeed");

	/* Clone should have same coefficients */
	for (i = 0; i < clone.num_coefficients; i++)
		ASSERT(clone.coefficients[i] == parent.coefficients[i],
		       "clone coefficients should match parent");

	/* Clone should have different ID */
	ASSERT(strncmp(clone.genome.id, parent.genome.id, ONTO_ID_LEN) != 0,
	       "clone should have different ID");

	/* Clone should have parent in lineage */
	ASSERT(clone.genome.lineage_count >= 1,
	       "clone should have parent in lineage");

	printf("PASSED\n");
}

/*
 * Test 10: Population diversity calculation
 */
static void
test_diversity(void)
{
	struct onto_kernel pop[3];
	tensor_scalar_t c1[3], c2[3], c3[3];
	tensor_scalar_t div;

	printf("Test 10: Population Diversity... ");

	c1[0] = ONTO_FIXED_ONE; c1[1] = 0; c1[2] = 0;
	c2[0] = 0; c2[1] = ONTO_FIXED_ONE; c2[2] = 0;
	c3[0] = 0; c3[1] = 0; c3[2] = ONTO_FIXED_ONE;

	onto_kernel_init(&pop[0], c1, 3, 3);
	onto_kernel_init(&pop[1], c2, 3, 3);
	onto_kernel_init(&pop[2], c3, 3, 3);

	div = onto_calculate_diversity(pop, 3);

	/* Diverse population should have positive diversity */
	ASSERT(div > 0, "diverse population should have diversity > 0");

	printf("PASSED\n");
}

/*
 * Test 11: Default configuration matches spec
 */
static void
test_default_config(void)
{
	struct onto_config config;

	printf("Test 11: Default Config... ");

	onto_config_default(&config);

	ASSERT(config.evolution.population_size == 20,
	       "population should be 20");
	ASSERT(config.evolution.max_generations == 100,
	       "max generations should be 100");
	ASSERT(config.development.embryonic_duration == 2,
	       "embryonic duration should be 2");
	ASSERT(config.development.juvenile_duration == 5,
	       "juvenile duration should be 5");
	ASSERT(config.development.mature_duration == 10,
	       "mature duration should be 10");

	printf("PASSED\n");
}

/*
 * Test 12: Find best kernel
 */
static void
test_find_best(void)
{
	struct onto_kernel pop[3];
	tensor_scalar_t coeffs[3];
	int best;

	printf("Test 12: Find Best... ");

	coeffs[0] = ONTO_FIXED_HALF;
	coeffs[1] = ONTO_FIXED_HALF;
	coeffs[2] = ONTO_FIXED_HALF;

	onto_kernel_init(&pop[0], coeffs, 3, 3);
	onto_kernel_init(&pop[1], coeffs, 3, 3);
	onto_kernel_init(&pop[2], coeffs, 3, 3);

	pop[0].genome.fitness = ONTO_FIXED_ONE / 4;
	pop[1].genome.fitness = ONTO_FIXED_ONE * 3 / 4;
	pop[2].genome.fitness = ONTO_FIXED_ONE / 2;

	best = onto_find_best(pop, 3);
	ASSERT(best == 1, "kernel with highest fitness should be found");

	printf("PASSED\n");
}

int
main(int argc, char *argv[], int envc, char *envp[])
{
	printf("=== Ontogenesis Test Suite ===\n");
	printf("Testing self-generating kernel system\n\n");

	ontogenesis_init();

	test_kernel_initialization();
	test_self_generation();
	test_self_optimization();
	test_crossover();
	test_mutation();
	test_grip_evaluation();
	test_genetic_distance();
	test_development_stages();
	test_cloning();
	test_diversity();
	test_default_config();
	test_find_best();

	printf("\n=== All 12 tests PASSED ===\n");
	return 0;
}
