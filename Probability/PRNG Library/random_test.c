/*
* Author: Biren Patel
* Description: Unit and integration tests for random number generator library.
* Most of these tests are slow as monte carlo simulations are required in many
* cases to approximately verify function behavior.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "timeit.h"
#include "random.h"
#include "src\unity.h"

/******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

#define self &rng.state
#define BIG_SIMULATION 2500000
#define MID_SIMULATION 500000
#define SMALL_SIMULATION 50000

/*******************************************************************************
Check that rng_bias is correct by monte carlo simulation on probabilites of
1/256 through 255/256. At 1,000,000 simulations with floating precision, the
tolerance is set as +/- 0.0015.
*/

void test_monte_carlo_of_rng_bias_at_256_bits_of_resolution(void)
{
    //arrange
    struct tuple {float actual; float expected;} result[255];
    random_t rng = rng_init(0, 255);
    assert(rng.state != 0 && "rdseed failure");
    float expected_counter = 0.0;
    
    //act
    for (size_t i = 0; i < 255; i++)
    {
        int success = 0;
        int numerator = i + 1;
        expected_counter += 0.00390625;
        
        for (size_t j = 0; j < BIG_SIMULATION; j++)
        {
            if (rng.bias(self, numerator, 8) & 1) success++;
        }
        
        result[i].actual = ((float) success) / BIG_SIMULATION;
        result[i].expected = expected_counter;
    }
    
    //assert
    for (size_t i = 0; i < 255; ++i)
    {        
        TEST_ASSERT_FLOAT_WITHIN(.001f, result[i].expected, result[i].actual);
    }        
}

/*******************************************************************************
Given an input stream with bits biased to .125 probability of success, output
a stream of 135 bits with unbiased bits. The input stream has no autocorrelation
and 135 was selected as a non-multiple of 64.
*/

void test_von_neumann_debiaser_outputs_all_unbiased_bits(void)
{
    //arrange
    random_t rng = rng_init(0, 255);
    assert(rng.state != 0 && "rdseed failure");
    
    uint64_t input_stream[35]; //2240 input bits should be enough
    uint64_t output_stream[3]; //space for 192 but only care about first 135
    float results[135] = {0}; //holds monte carlo result for each output bit
    
    stream_t info;
    double avg_use = 0;
    uint64_t min_use = UINT64_MAX;
    uint64_t max_use = 0;
    
    //act-assert
    for (size_t i = 0; i < MID_SIMULATION; i++)
    {
        //populate the input stream with biased bits
        for (size_t j = 0; j < 35; ++j)
        {
            input_stream[j] = rng.bias(self, 32, 8);
        }
        
        //populate the output stream with unbiased bits
        info = rng.vndb(input_stream, output_stream, 2240, 135);
        TEST_ASSERT_EQUAL_INT(135, info.filled);
        
        //ancillary points of interest
        avg_use += info.used;
        if (info.used < min_use) min_use = info.used;
        if (info.used > max_use) max_use = info.used;
        
        //walk through the output stream and assess results of this round
        for (size_t k = 0; k < 135; k++)
        {
            if ((output_stream[k/64] >> (k % 64)) & 1) results[k]++;
        }
    }
    
    //assert
    for (size_t i = 0; i < 135; ++i)
    {
        results[i] /= MID_SIMULATION;
        TEST_ASSERT_FLOAT_WITHIN(.01f, 0.5f, results[i]);
    }
    
    //points of interest
    puts("POI: test_von_neumann_debiaser_outputs_all_unbiased_bits");
    printf("average input bits used: %-10g\n", avg_use/MID_SIMULATION);
    printf("maximum input bits used: %llu\n", max_use);
    printf("minimum input bits used: %llu\n", min_use);
}

/******************************************************************************/

void test_cyclic_autocorrelation_of_bitstream(void)
{
    //arrange
    random_t rng = rng_init(0, 255);
    assert(rng.state != 0 && "rdseed failure");
    
    uint64_t input_stream[100000] = {0};
    uint64_t weyl = 0xAAAAAAAAAAAAAAAA;
    
    for (size_t i = 0; i < 100000; i++)
    {
        input_stream[i] = weyl;
        //weyl += 0xDEADBEEF;
    }
    
    //act
    for (size_t i = 0; i < 64; i++)
    {
        printf("%g\n", rng.cycc(input_stream, 6400000, i));
    }
}

/******************************************************************************/

int main(void)
{
    UNITY_BEGIN();
        RUN_TEST(test_monte_carlo_of_rng_bias_at_256_bits_of_resolution);
        RUN_TEST(test_von_neumann_debiaser_outputs_all_unbiased_bits);
    UNITY_END();
    
    
    test_cyclic_autocorrelation_of_bitstream();
    
    return EXIT_SUCCESS;
}
