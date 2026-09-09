#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include <stdint.h>
typedef int fixed_t;

#define FP_SHIFT 14
#define FP_ONE (1 << FP_SHIFT)

/* Convert integer to fixed point. */
static inline fixed_t
int_to_fp (int n)
{
  return n * FP_ONE;
}

/* Convert fixed point to integer, rounding toward zero. */
static inline int
fp_to_int_trunc (fixed_t x)
{
  return x / FP_ONE;
}

/* Convert fixed point to integer, rounding to nearest. */
static inline int
fp_to_int_round (fixed_t x)
{
  if (x >= 0)
    return (x + FP_ONE / 2) / FP_ONE;
  else
    return (x - FP_ONE / 2) / FP_ONE;
}

/* Add two fixed point numbers. */
static inline fixed_t
fp_add (fixed_t x, fixed_t y)
{
  return x + y;
}

/* Subtract fixed point y from x. */
static inline fixed_t
fp_sub (fixed_t x, fixed_t y)
{
  return x - y;
}

/* Add fixed point x and integer n. */
static inline fixed_t
fp_add_int (fixed_t x, int n)
{
  return x + n * FP_ONE;
}

/* Subtract integer n from fixed point x. */
static inline fixed_t
fp_sub_int (fixed_t x, int n)
{
  return x - n * FP_ONE;
}

/* Multiply two fixed point numbers. */
static inline fixed_t
fp_mul (fixed_t x, fixed_t y)
{
  return ((int64_t) x) * y / FP_ONE;
}

/* Multiply fixed point x by integer n. */
static inline fixed_t
fp_mul_int (fixed_t x, int n)
{
  return x * n;
}

/* Divide fixed point x by fixed point y. */
static inline fixed_t
fp_div (fixed_t x, fixed_t y)
{
  return ((int64_t) x) * FP_ONE / y;
}

/* Divide fixed point x by integer n. */
static inline fixed_t
fp_div_int (fixed_t x, int n)
{
  return x / n;
}

#endif /* threads/fixed-point.h */
