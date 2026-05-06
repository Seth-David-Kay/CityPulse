// https://github.com/felix-u 2026-02-12
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(PRNG_H)
#define PRNG_H


typedef struct {
    unsigned long long seed[4];
} prng_State;

#if !defined(PRNG_FUNCTION)
    #define PRNG_FUNCTION
#endif

PRNG_FUNCTION prng_State         prng_init(unsigned long long seed);
// 1 included, 2 excluded
PRNG_FUNCTION double             prng_next_f64_between_1_and_2(prng_State *state);
PRNG_FUNCTION unsigned long long prng_next_u64(prng_State *state);


#endif // PRNG_H


#if defined(PRNG_IMPLEMENTATION)


#if !defined(PRNG_ASSERT)
    #include <assert.h>
    #define PRNG_ASSERT assert
#endif

#if !defined(PRNG_MEMCPY)
    #define PRNG_MEMCPY prng__memcpy
    static inline void *prng__memcpy(void *destination, const void *source, unsigned long long count) {
        for (unsigned long long i = 0; i < count; i += 1) ((char *)destination)[i] = ((const char *)source)[i];
        return destination;
    }
#endif

// based on https://prng.di.unimi.it/splitmix64.c, which is also in the public domain
PRNG_FUNCTION prng_State prng_init(unsigned long long seed) {
    prng_State result = {0};

    for (int i = 0; i < 4; i += 1) {
        seed += 0x9e3779b97f4a7c15;
        unsigned long long z = seed;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        result.seed[i] = z ^ (z >> 31);
    }

    return result;
}

static unsigned long long prng__rotate(unsigned long long x, int k) {
    return (x << k) | (x >> (64 - k));
}

// based on https://prng.di.unimi.it/xoshiro256plus.c, which is also in the public domain
PRNG_FUNCTION double prng_next_f64_between_1_and_2(prng_State *state) {
    unsigned long long *s = state->seed;
    PRNG_ASSERT((s[0] != 0 || s[1] != 0 || s[2] != 0 || s[3] != 0) && "you didn't seed the PRGN");

    unsigned long long bits = s[0] + s[3];

    unsigned long long t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = prng__rotate(s[3], 45);

    bits = (bits >> 12) | 0x3ff0000000000000;
    double result;
    PRNG_MEMCPY(&result, &bits, sizeof result);
    return result;
}

// based on https://prng.di.unimi.it/xoshiro256plusplus.c, which is also in the public domain
PRNG_FUNCTION unsigned long long prng_next_u64(prng_State *state) {
    unsigned long long *s = state->seed;
    PRNG_ASSERT((s[0] != 0 || s[1] != 0 || s[2] != 0 || s[3] != 0) && "you didn't seed the PRGN");

    unsigned long long result = prng__rotate(s[0] + s[3], 23) + s[0];

    unsigned long long t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = prng__rotate(s[3], 45);

    return result;
}


#endif // PRNG_IMPLEMENTATION
