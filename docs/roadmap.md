# Status And Extensions

## Implemented In This Repository

- dialect-aware HLFIR/FIR matching when built with Flang support
- guarded `fir.allocmem` to `fir.alloca` stack promotion for bounded compiler temporaries
- direct `fir.do_loop` scalarization for safe `hlfir.elemental` assignments, including nested and higher-rank cases
- stable text, JSON, SARIF, DOT, and profile-site CSV reporting
- real `.f90 -> Flang HLFIR -> FIAP` pipeline
- thirteen real Fortran test cases with positive, negative, descriptor, section, pointer-alias, rank-3, and larger-kernel coverage
- CTest evidence gates and release packaging scripts

## Research Extensions Beyond The Submission

- interprocedural shape invariant propagation
- live-range aware scratch-buffer reuse
- region-based temporary coalescing across adjacent array expressions

## Optional Evaluation Extensions

- SPEC CPU Fortran workloads
- LAPACK kernels
- climate and CFD mini-apps
- compiler overhead measurements per KLOC
