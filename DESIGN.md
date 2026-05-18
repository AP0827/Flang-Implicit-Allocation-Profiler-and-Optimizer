# DESIGN

## Goal

The tool finds implicit heap allocations that Flang introduces while lowering Fortran through HLFIR/FIR. The target constructs are:

- array-expression temporaries, for example `A = B + C`
- elemental expression temporaries
- array-valued function results
- automatic realloc-on-assignment for allocatable arrays
- temporary lifetime extension through `hlfir.associate`

The user-facing result is a source-mapped report that explains where the allocation came from, how large it appears to be, whether it can be eliminated, and what rewrite is appropriate.

## Approach

The core design is an Allocation Provenance Graph (APG). Each relevant HLFIR/FIR operation becomes a node with:

- operation kind and construct kind
- source location
- shape and byte estimate
- loop depth
- escape status
- producer and consumer counts
- classification and rewrite advice

Edges record provenance:

- `produces`: one operation creates a value consumed by another operation
- `consumes`: an assignment, call, or cleanup consumes a temporary
- `shape-constrains`: one operation constrains the shape of another
- `aliases`: multiple users may observe the same temporary
- `lifetime-ends`: cleanup/destroy relationship

The classifier uses a conservative lattice:

1. `provably-eliminable`: local, non-escaping, single-consumer, shape-bounded sites.
2. `possibly-unnecessary`: allocation may be removable but needs runtime shape or user intent.
3. `necessary`: current evidence says local removal is unsafe.

## Alternatives Considered

### Source-only analysis

A source-only Fortran parser can find array syntax, but it cannot reliably identify whether Flang materializes heap storage. The same source expression can lower differently depending on rank, shape, aliases, and procedure boundaries. This was rejected as the primary approach.

### Runtime allocation interposition

Intercepting malloc/free can measure allocation volume, but it cannot explain the source construct or suggest compiler-level rewrites. It is useful for validation but not for precise attribution.

### Fully dialect-specific Flang pass only

Typed FIR/HLFIR matching is best when exact Flang headers are available, but classroom machines may not have a complete Flang build. The implemented design supports typed matching when `Flang_DIR` is provided and falls back to generic MLIR operation matching for demo/test portability.

## Transformation Strategy

The C++ pass attaches machine-readable `fiap.*` metadata and transformation hints. The simple source transformer consumes a JSON report and rewrites straightforward rank-1 assignments into explicit `do concurrent` loops. More complex rewrites are prepared in IR using attributes such as `fiap.rewrite_template` and `fiap.lowering_hint` so the prototype remains robust across Flang revisions.

## Profile-Guided Refinement

The profile-guided allocation elimination loop is implemented as a report refinement stage. The static pass emits JSON entries for ambiguous sites. A profile CSV records observed allocation counts, observed bytes, and whether runtime shapes were stable. `scripts/refine_profile.py` joins the profile back to source locations and upgrades matching `possibly-unnecessary` sites to `provably-eliminable` under a profile-validated shape invariant.

This keeps the core static pass conservative while still demonstrating the research idea: additional evidence can monotonically lower a node in the allocation lattice from ambiguous to eliminable.

## Failure Cases

The tool intentionally reports conservative results for:

- dynamic shapes without proof of equality
- temporaries escaping through calls or returns
- multiple consumers or aliasing
- unknown operations unless explicitly requested

These are shown in the demo as `possibly-unnecessary` or `necessary` instead of being silently transformed.
