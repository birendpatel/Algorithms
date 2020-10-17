/*
* Author: Biren Patel
* Description: PRNG library utilities
*/

#ifndef RANDOM_H
#error "include random.h instead of random_utils.h"
#endif

#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

#include <immintrin.h>
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
* NAME: rdrand
* DESC: Retry loop for x86 rdrand instruction
* OUTP: false if rdrand could not generate a number within 10 attempts
* @ x : contains valid random number if and only if the function returns true
*******************************************************************************/
bool rdrand(uint64_t *x);

/*******************************************************************************
* NAME: rng_hash
* DESC: integer hashing function
* OUTP: unsigned 64-bit hash
* @ value : returned integer is the hash of input value
*******************************************************************************/
uint64_t rng_hash (uint64_t value);

#endif