#pragma once

#include "fiap/AnalysisOptions.h"

#include "mlir/Pass/Pass.h"

#include <memory>

namespace fiap {

std::unique_ptr<mlir::Pass>
createImplicitAllocationProfilerPass(ProfilerPassOptions options = {});
std::unique_ptr<mlir::Pass> createPromoteTempToStackPass();
std::unique_ptr<mlir::Pass> createScalarizeArrayExprPass();
void registerFIAPPasses();

} // namespace fiap
