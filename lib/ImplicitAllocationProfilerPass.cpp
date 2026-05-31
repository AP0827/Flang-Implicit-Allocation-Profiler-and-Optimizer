#include "fiap/AllocationClassifier.h"
#include "fiap/AllocationReport.h"
#include "fiap/ImplicitAllocationAnalysis.h"
#include "fiap/Passes.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

using namespace fiap;

namespace {

void annotateNode(APGNode &node) {
  if (node.op == nullptr) {
    return;
  }

  mlir::MLIRContext *context = node.op->getContext();
  node.op->setAttr("fiap.classification",
                   mlir::StringAttr::get(context, toString(node.classification)));
  node.op->setAttr("fiap.construct",
                   mlir::StringAttr::get(context, toString(node.construct)));
  node.op->setAttr("fiap.escape",
                   mlir::StringAttr::get(context, toString(node.escape)));
  node.op->setAttr("fiap.transform",
                   mlir::StringAttr::get(context, toString(node.suggestedTransform)));
  node.op->setAttr("fiap.transformable",
                   mlir::BoolAttr::get(context, node.transformable));
  node.op->setAttr("fiap.alias_risk",
                   mlir::BoolAttr::get(context, node.aliasRisk));
  node.op->setAttr("fiap.typed_flang_match",
                   mlir::BoolAttr::get(context, node.typedFlangMatch));
  node.op->setAttr("fiap.loop_depth",
                   mlir::IntegerAttr::get(mlir::IntegerType::get(context, 32), node.loopDepth));
  if (node.estimate.byteCount) {
    node.op->setAttr("fiap.estimated_bytes", mlir::IntegerAttr::get(
                                                 mlir::IntegerType::get(context, 64),
                                                 *node.estimate.byteCount));
  }
  node.op->setAttr("fiap.reason", mlir::StringAttr::get(context, node.reason));
  node.op->setAttr("fiap.advice", mlir::StringAttr::get(context, node.advice));
  if (!node.legality.empty()) {
    node.op->setAttr("fiap.legality",
                     mlir::StringAttr::get(context, node.legality));
  }
  if (!node.legalityReason.empty()) {
    node.op->setAttr("fiap.legality_reason",
                     mlir::StringAttr::get(context, node.legalityReason));
  }
  if (!node.shapeEvidence.empty()) {
    node.op->setAttr("fiap.shape_evidence",
                     mlir::StringAttr::get(context, node.shapeEvidence));
  }
  if (!node.aliasEvidence.empty()) {
    node.op->setAttr("fiap.alias_evidence",
                     mlir::StringAttr::get(context, node.aliasEvidence));
  }
  switch (node.suggestedTransform) {
  case TransformKind::ScalarizeToLoopNest:
    node.op->setAttr("fiap.rewrite_status",
                     mlir::StringAttr::get(context, "prepared-for-scalarization"));
    node.op->setAttr("fiap.rewrite_template",
                     mlir::StringAttr::get(
                         context,
                         "do concurrent over destination shape; compute rhs element directly into lhs"));
    break;
  case TransformKind::PromoteToStack:
    node.op->setAttr("fiap.rewrite_status",
                     mlir::StringAttr::get(context, "prepared-for-stack-promotion"));
    node.op->setAttr("fiap.lowering_hint",
                     mlir::StringAttr::get(context, "replace fir.allocmem with fir.alloca"));
    break;
  case TransformKind::PreallocateLHS:
    node.op->setAttr("fiap.rewrite_status",
                     mlir::StringAttr::get(context, "prepared-for-result-buffering"));
    node.op->setAttr("fiap.rewrite_template",
                     mlir::StringAttr::get(context, "rewrite array-valued function as explicit result buffer"));
    break;
  case TransformKind::AddShapeGuard:
    node.op->setAttr("fiap.rewrite_status",
                     mlir::StringAttr::get(context, "prepared-for-shape-guard"));
    node.op->setAttr("fiap.rewrite_template",
                     mlir::StringAttr::get(context, "guard allocation with allocated/shape checks"));
    break;
  case TransformKind::None:
    break;
  }
}

class ImplicitAllocationProfilerPass
    : public mlir::PassWrapper<ImplicitAllocationProfilerPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ImplicitAllocationProfilerPass)

  explicit ImplicitAllocationProfilerPass(ProfilerPassOptions options = {})
      : options_(std::move(options)) {}

  void runOnOperation() override {
    mlir::ModuleOp module = getOperation();

    ImplicitAllocationAnalysis analysis(options_.analysis);
    AllocationProvenanceGraph graph = analysis.build(module);

    AllocationClassifier classifier;
    classifier.classify(graph);

    if (options_.annotateIR) {
      for (APGNode &node : graph.nodes()) {
        annotateNode(node);
      }
    }

    AllocationReport report =
        AllocationReport::fromGraph(graph, options_.includeNonAllocationNodes);
    llvm::outs() << report.render(options_.reportFormat, &graph, options_.emitSummary);

    if (options_.printAnnotatedIR) {
      llvm::outs() << "\n";
      module.print(llvm::outs());
      llvm::outs() << "\n";
    }
  }

  llvm::StringRef getArgument() const final {
    return "fiap-implicit-allocation-profiler";
  }

  llvm::StringRef getDescription() const final {
    return "Detects and classifies implicit HLFIR/FIR allocation sites";
  }

private:
  ProfilerPassOptions options_;
};

} // namespace

std::unique_ptr<mlir::Pass>
fiap::createImplicitAllocationProfilerPass(ProfilerPassOptions options) {
  return std::make_unique<ImplicitAllocationProfilerPass>(std::move(options));
}

void fiap::registerFIAPPasses() {
  static const bool registered = [] {
    static mlir::PassRegistration<ImplicitAllocationProfilerPass> profiler;
    static mlir::PassPipelineRegistration<> pipeline(
        "fiap-profile-and-transform",
        "Run FIAP implicit-allocation profiling followed by guarded HLFIR/FIR rewrites",
        [](mlir::OpPassManager &pm) {
          ProfilerPassOptions options;
          options.annotateIR = true;
          options.emitSummary = false;
          pm.addPass(createImplicitAllocationProfilerPass(options));
          pm.addPass(createPromoteTempToStackPass());
          pm.addPass(createScalarizeArrayExprPass());
        });
    (void)profiler;
    (void)pipeline;
    return true;
  }();
  (void)registered;
}
