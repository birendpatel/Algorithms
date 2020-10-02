/*
* Author: Biren Patel
* Description: Unit and integration tests for random number generator library.
*/

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "timeit.h"
#include "random.h"

bool test_monte_carlo_of_all_rng_bias_resolutions_is_approximately_correct(void)
{
    //arrange: test all 256 resolutions with 1 million simulations each
    enum {SAMPLES = 1000000};
    enum {RESOLUTION_MAX = 256};
    double result[RESOLUTION_MAX];
    
    random_t rng = rng_init(0, 100);
    if (rng.state == 0)
    {
        fprintf(stderr, "rdseed instruction failed\n");
        return false;
    }
    
    //act: at each resolution, generate a biased number and check any bit.
    //since the operation is "vectorized" and the PRNG is assumed "unbiased",
    //it doesn't matter (in theory) which bit is checked, so I use bit 0.
    
    for (size_t i = 0; i < RESOLUTION_MAX; ++i)
    {
        int success = 0;
        
        for (size_t j = 0; j < SAMPLES; ++j)
        {
            if (rng.bias(&rng.state, i) & 0x1) ++success;
        }
        
        result[i] = ((double) success) / ((double) SAMPLES);
    }
    
    //assert (just interested in seeing the visuals for now)
    double base = 0.0;
    
    for (size_t i = 0; i < RESOLUTION_MAX; ++i)
    {
        printf("%llu: (actual) %-10g \t (true) %10g\n", i, result[i], base);
        base += 0.00390625; //a little floating imprecision but good enough
    }
    
    return true;
}

/******************************************************************************/

void test_compare_execution_time_of_rng_generator_to_rng_bias(void)
{
    random_t rng = rng_init(0, 100);
    if (rng.state == 0)
    {
        fprintf(stderr, "rdseed instruction failed\n");
        abort();
    }
    
    init_timeit();
    
    start_timeit();
    
    for (size_t i = 0; i < 1000000; ++i)
    {
        rng.next(&rng.state);
    }
    
    end_timeit();
    
    printf("xorshift: %lld microseconds\n", TIMEIT_RESULT_MICROSECONDS);
    
    start_timeit();
    
    for (size_t i = 0; i < 1000000; ++i)
    {
        rng.bias(&rng.state, 1);
    }
    
    end_timeit();
    
    printf("biased: %lld micrseconds\n", TIMEIT_RESULT_MICROSECONDS);
}

int main(void)
{
    //test_monte_carlo_of_all_rng_bias_resolutions_is_approximately_correct();
    
    test_compare_execution_time_of_rng_generator_to_rng_bias();
    
    return EXIT_SUCCESS;
}