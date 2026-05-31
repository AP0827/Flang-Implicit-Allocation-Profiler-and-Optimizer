# Correctness Argument

This document states the local correctness argument for FIAP's implemented
rewrites. It is intentionally scoped to the transformations that the tool
actually applies.

## Scalarizing `hlfir.elemental` Assignments

FIAP rewrites an elemental temporary into an explicit loop nest only when all of
the following preconditions hold:

1. The candidate operation is an `hlfir.elemental` array expression.
2. The expression is classified `provably-eliminable`.
3. The classifier marks the site `legal-for-rewrite`.
4. The expression has exactly one non-destroy consumer.
5. That consumer is an `hlfir.assign` whose RHS is the expression result.
6. The assignment does not request reallocating assignment.
7. The LHS is a Fortran entity that can be indexed with HLFIR helpers.
8. The value does not escape to a call, return, or associate region.
9. Conservative alias evidence is absent.
10. The expression rank is a valid Fortran rank.

Under those preconditions, FIAP generates a `fir.do_loop` nest over the
elemental shape, inlines the elemental body for each one-based index tuple, and
assigns the scalar element directly into the corresponding LHS element.

The rewrite preserves the pointwise semantics of the original elemental
assignment because each generated loop iteration computes the same yielded
element value for the same index tuple and stores it into the same destination
element. The removed `hlfir.destroy` operations are safe to erase because the
temporary expression object no longer exists after direct scalarization.

## Stack Promotion

FIAP rewrites `fir.allocmem` to `fir.alloca` only when all of the following hold:

1. The allocation is classified `provably-eliminable`.
2. The suggested transform is `promote-to-stack`.
3. The legality state is `legal-for-rewrite`.
4. The allocation is not marked `fir.must_be_heap`.
5. The allocation is not loop-local.
6. The allocation has no dynamic shape operands or length parameters.
7. The byte estimate is statically bounded and under FIAP's promotion limit.

The replacement uses `fir.alloca` for storage and a type-compatible `fir.convert`
bridge for existing uses. Direct `fir.freemem` users are erased because stack
storage lifetime is bound to the surrounding frame rather than explicit heap
deallocation.

## Refused Cases

FIAP refuses local rewriting for cases that violate the above preconditions,
including escaping temporaries, pointer/descriptor/class-like alias-sensitive
expressions, reallocating assignment, dynamic heap-required storage, and cases
with multiple non-destroy consumers. These cases remain reportable, but their
legality state prevents transform passes from applying rewrites.
