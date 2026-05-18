#pragma once

#include "fiap/AnalysisOptions.h"
#include "fiap/APG.h"

#include <string>

namespace mlir {
class DialectRegistry;
class Operation;
} // namespace mlir

namespace fiap {

struct OperationSemantics {
  bool interesting = false;
  bool compilerGenerated = false;
  bool returnsArrayLike = false;
  bool typedFlangMatch = false;
  APGNodeKind kind = APGNodeKind::Unknown;
  ImplicitConstructKind construct = ImplicitConstructKind::Unknown;
};

bool compiledWithFlangSupport();
void registerProjectDialects(mlir::DialectRegistry &registry);

OperationSemantics classifyOperationSemantics(mlir::Operation &op,
                                              const AnalysisOptions &options);
ShapeInfo inferShapeInfo(mlir::Operation &op);
std::string summarizeOperation(mlir::Operation &op,
                               const OperationSemantics &semantics);

} // namespace fiap
