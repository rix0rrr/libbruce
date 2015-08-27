#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <libbruce/bruce.h>

/**
 * Return the size of a 32-bit int
 */
uint32_t intSize(const void *);
int intCompare(const bruce::memory &, const bruce::memory &);

int rngcmp(const bruce::memory &a, const bruce::memory &b);

extern bruce::tree_functions intToIntTree;

extern bruce::memory one_r;
extern bruce::memory two_r;
extern bruce::memory three_r;

#endif
