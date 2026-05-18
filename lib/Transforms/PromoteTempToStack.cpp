#include "fiap/Passes.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace fiap {
namespace {

class PromoteTempToStackPass
    : public mlir::PassWrapper<PromoteTempToStackPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PromoteTempToStackPass)

  void runOnOperation() override {
    mlir::ModuleOp module = getOperation();
    module.walk([&](mlir::Operation *op) {
      if (!op->getName().getStringRef().contains("fir.allocmem")) {
        return;
      }

      auto classification = op->getAttrOfType<mlir::StringAttr>("fiap.classification");
      auto transform = op->getAttrOfType<mlir::StringAttr>("fiap.transform");
      if (classification == nullptr || transform == nullptr) {
        return;
      }

      if (classification.getValue() != "provably-eliminable" ||
          transform.getValue() != "promote-to-stack") {
        return;
      }

      mlir::MLIRContext *context = op->getContext();
      op->setAttr("fiap.lowering_hint",
                  mlir::StringAttr::get(context, "replace fir.allocmem with fir.alloca"));
      op->setAttr("fiap.rewrite_status",
                  mlir::StringAttr::get(context, "prepared-for-stack-promotion"));
      op->emitRemark("fiap prepared this allocation for stack promotion");
    });
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createPromoteTempToStackPass() {
  return std::make_unique<PromoteTempToStackPass>();
}

} // namespace fiap
