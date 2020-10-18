/*
* Author: Biren Patel
* Description: PRNG library implementation
*/

#include "random.h"

/*******************************************************************************
Permuted Congruential Generator from Melissa O'Neill. This is the insecure 32
bit output PCG extracted from O'Neill's C implementation on pcg_random.org.
I have refactored the generator to use Intel AVX2 instruction set intrinsics. 
Four independent streams are updated and permuted simultaneously on the YMM
registers through 4 64-bit blocks. The upper 32 bits of each block allow space 
for the LCG, hence why the insecure algorithm is used.
*/

static __m256i simd_rng_generator_partial
(
    simd_state_t * const state
)
{
    const __m256i lcg_mult = _mm256_set1_epi64x((int64_t) 0x2C9277B5U);
    const __m256i rxs_mult = _mm256_set1_epi64x((int64_t) 0x108EF2D9U);
    const __m256i mod_mask = _mm256_set1_epi64x((int64_t) 0xFFFFFFFFU);
    
    __m256i x  = state->current;
    __m256i fx = _mm256_setzero_si256();
    
    fx = _mm256_add_epi32(_mm256_srli_epi32(x, 28), _mm256_set1_epi32(4LL));
    fx = _mm256_srlv_epi32(x, fx);
    fx = _mm256_xor_si256(x, fx);
    fx = _mm256_mul_epu32(fx, rxs_mult);
    fx = _mm256_and_si256(fx, mod_mask);
    fx = _mm256_xor_si256(_mm256_srli_epi32(fx, 22), fx);    

    state->current = _mm256_mul_epu32(state->current, lcg_mult);
    state->current = _mm256_and_si256(state->current, mod_mask);
    state->current = _mm256_add_epi64(state->current, state->increment);
    state->current = _mm256_and_si256(state->current, mod_mask);
    
    return fx;
}

/*******************************************************************************
This function is a decorator. PCG32i limited to AX2 instructions can only fill
128 bits per vector. This function runs each stream twice so that in total we
output 256 bits. Each 64 bit block is generated by one stream, where the first
number in the stream sequence is contained in the lower 32 bits, and the second 
numer is contained in the upper 32 bits. This function absolutely depends on the
upper bits in the lower vector being nonzero, which is guaranteed by the modmask
during the permutation step.
*/

__m256i simd_rng_generator
(
    simd_state_t * const state
)
{
    __m256i lower;
    __m256i upper;
    __m256i output;
    
    lower = simd_rng_generator_partial(state);
    upper = simd_rng_generator_partial(state);
    upper = _mm256_slli_epi64(upper, 32);
    output = _mm256_or_si256(upper, lower);
    
    return output;
}

/*******************************************************************************
This it the initialization function for the AVX2 API. ALmost the same as 64-Bit
but 4 seed parameters make the initialization of each PCG stream easier. It also
allows for easier debugging as we can initialize a non-SIMD PCG32i and follow
each "thread" individually. See the unit tests for an example. Since RDRAND 
needs 10 retries, the 80 total attempts are split in 10-try blocks with goto.
Like a single-stream PCG, the increment parameter must be odd for all streams.
The upper 32 bits of each 64 bit block are cleared in the state and increment
vectors as a safety measure. In the generator we don't want to accidentally
shift in useless bits during the permutation step if we were using epi64 funcs
instead of epi32.
*/

simd_random_t simd_rng_init
(
    const uint64_t seed_1,
    const uint64_t seed_2,
    const uint64_t seed_3,
    const uint64_t seed_4
)
{
    simd_random_t simd_rng;
    const __m256i mask = _mm256_set1_epi64x((int64_t) 0xFFFFFFFFU);
    const __m256i odd = _mm256_set1_epi64x((int64_t) 0x1U);
    
    uint64_t LL;
    uint64_t LH;
    uint64_t HL;
    uint64_t HH;
    
    if (seed_1 != 0 && seed_2 != 0 && seed_3 != 0 && seed_4 != 0)
    {
        LL = rng_hash(seed_4);
        LH = rng_hash(seed_3);
        HL = rng_hash(seed_2);
        HH = rng_hash(seed_1);
        
        simd_rng.state.current = _mm256_set_epi64x
        (
            (int64_t) LL,
            (int64_t) LH,
            (int64_t) HL,
            (int64_t) HH                          
        );
             
        LL = rng_hash(LL);
        LH = rng_hash(LH);
        HL = rng_hash(HL);
        HH = rng_hash(HH);
                         
        simd_rng.state.increment = _mm256_set_epi64x
        (
            (int64_t) LL,
            (int64_t) LH,
            (int64_t) HL,
            (int64_t) HH  
        );
    }
    else
    {
        if (rdrand(&LL) && rdrand(&LH) && rdrand(&HL) && rdrand(&HH))
        {
            simd_rng.state.current = _mm256_set_epi64x
            (
                (int64_t) LL,
                (int64_t) LH,
                (int64_t) HL,
                (int64_t) HH                          
            );
            
            goto first_pass_success;
        }
        goto fail;
        
        first_pass_success:
        
        if (rdrand(&LL) && rdrand(&LH) && rdrand(&HL) && rdrand(&HH))
        {
            simd_rng.state.increment = _mm256_set_epi64x
            (
                (int64_t) LL,
                (int64_t) LH,
                (int64_t) HL,
                (int64_t) HH  
            );
            
            goto success;
        }
        
        fail:
            simd_rng.state.current = _mm256_setzero_si256();
            simd_rng.state.increment = _mm256_setzero_si256();
            goto terminate;
    }
    
    success:
        simd_rng.state.current = _mm256_and_si256(simd_rng.state.current, mask);
        simd_rng.state.increment = _mm256_and_si256(simd_rng.state.increment, mask);
        simd_rng.state.increment = _mm256_or_si256(simd_rng.state.increment, odd);
        simd_rng.next = simd_rng_generator;
    
    terminate:
        return simd_rng;
}
