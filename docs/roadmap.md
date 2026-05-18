# Roadmap

## Near-Term Engineering

- replace string-based FIR/HLFIR matching with dialect-aware operation matchers
- materialize exact `fir.alloca` rewrites where bounded shape proofs hold
- scalarize rank-1 `hlfir.assign` expressions with direct FIR loop generation
- emit stable JSON schemas for benchmark ingestion

## Research Extensions

- interprocedural shape invariant propagation
- profile-guided promotion from `possibly-unnecessary` to `provably-eliminable`
- live-range aware scratch-buffer reuse
- region-based temporary coalescing across adjacent array expressions

## Evaluation Extensions

- SPEC CPU Fortran workloads
- LAPACK kernels
- climate and CFD mini-apps
- compiler overhead measurements per KLOC
