/* APPLE LOCAL 5308174 */
/* { dg-options "-fconstant-cfstrings -Os" } */
/* { dg-do compile { target *-*-darwin* } } */

@class NSString;

typedef int (*comparatorFn)(const void *val1, const void *val2, void *context);

int compareFn1(const void *msg1, const void *msg2, void *context);

typedef struct {
    NSString *name;
    comparatorFn func;
} ComparatorPair;

static const ComparatorPair comparators[1] = {
    { @"function1", (comparatorFn)&compareFn1 },
};

comparatorFn getCompareFn(void) {
  ComparatorPair comp = comparators[0];

  return comp.func;
}

