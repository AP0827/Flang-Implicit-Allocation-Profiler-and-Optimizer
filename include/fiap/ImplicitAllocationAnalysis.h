#pragma once

#include "fiap/AnalysisOptions.h"
#include "fiap/APG.h"

namespace mlir {
class ModuleOp;
} // namespace mlir

namespace fiap {

class ImplicitAllocationAnalysis {
public:
  explicit ImplicitAllocationAnalysis(AnalysisOptions options = {});

  AllocationProvenanceGraph build(mlir::ModuleOp module) const;

private:
  AnalysisOptions options_;
};

} // namespace fiap
