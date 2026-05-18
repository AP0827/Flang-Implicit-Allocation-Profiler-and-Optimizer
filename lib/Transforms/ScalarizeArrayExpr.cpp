#include "fiap/Passes.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace fiap {
namespace {

class ScalarizeArrayExprPass
    : public mlir::PassWrapper<ScalarizeArrayExprPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ScalarizeArrayExprPass)

  void runOnOperation() override {
    mlir::ModuleOp module = getOperation();
    module.walk([&](mlir::Operation *op) {
      auto classification = op->getAttrOfType<mlir::StringAttr>("fiap.classification");
      auto transform = op->getAttrOfType<mlir::StringAttr>("fiap.transform");
      if (classification == nullptr || transform == nullptr) {
        return;
      }

      if (classification.getValue() != "provably-eliminable" ||
          transform.getValue() != "scalarize-to-loop-nest") {
        return;
      }

      mlir::MLIRContext *context = op->getContext();
      op->setAttr("fiap.rewrite_template", mlir::StringAttr::get(
                                               context,
                                               "do i = lbound(lhs,1), ubound(lhs,1); lhs(i) = rhs(i); end do"));
      op->setAttr("fiap.rewrite_status",
                  mlir::StringAttr::get(context, "prepared-for-scalarization"));
      op->emitRemark("fiap prepared this assignment for scalarization into an explicit loop nest");
    });
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createScalarizeArrayExprPass() {
  return std::make_unique<ScalarizeArrayExprPass>();
}

} // namespace fiap
