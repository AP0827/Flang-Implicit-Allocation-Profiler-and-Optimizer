#include "fiap/Passes.h"

#include "fiap/Config.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <string>

#if FIAP_HAVE_FLANG
#include "flang/Optimizer/Dialect/FIROps.h"
#endif

namespace fiap {
namespace {

bool hasStringAttr(mlir::Operation *op, llvm::StringRef name,
                   llvm::StringRef expected) {
  auto attr = op->getAttrOfType<mlir::StringAttr>(name);
  return attr && attr.getValue() == expected;
}

bool isStackPromotionCandidate(mlir::Operation *op) {
  return hasStringAttr(op, "fiap.classification", "provably-eliminable") &&
         hasStringAttr(op, "fiap.transform", "promote-to-stack") &&
         hasStringAttr(op, "fiap.legality", "legal-for-rewrite");
}

#if FIAP_HAVE_FLANG
bool isInsideLoop(mlir::Operation *op) {
  auto loopDepth = op->getAttrOfType<mlir::IntegerAttr>("fiap.loop_depth");
  return loopDepth && loopDepth.getInt() > 0;
}

bool canPromoteAllocMem(fir::AllocMemOp alloc) {
  mlir::MLIRContext *context = alloc.getContext();
  if (!isStackPromotionCandidate(alloc.getOperation())) {
    return false;
  }
  if (alloc->hasAttr("fir.must_be_heap")) {
    alloc->setAttr("fiap.rewrite_status",
                   mlir::StringAttr::get(context, "skipped-must-be-heap"));
    return false;
  }
  if (isInsideLoop(alloc.getOperation())) {
    alloc->setAttr("fiap.rewrite_status",
                   mlir::StringAttr::get(context, "skipped-loop-local-heap"));
    return false;
  }
  if (alloc.hasShapeOperands() || alloc.hasLenParams()) {
    alloc->setAttr("fiap.rewrite_status",
                   mlir::StringAttr::get(context, "skipped-dynamic-size"));
    return false;
  }
  return true;
}

bool promoteAllocMem(mlir::OpBuilder &builder, fir::AllocMemOp alloc) {
  if (!canPromoteAllocMem(alloc)) {
    return false;
  }

  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(alloc);

  std::string uniqName = "fiap.promoted.stack";
  if (std::optional<llvm::StringRef> name = alloc.getUniqName()) {
    uniqName = name->str() + ".stack";
  }

  auto alloca = fir::AllocaOp::create(builder, alloc.getLoc(), alloc.getInType(),
                                      uniqName, alloc.getTypeparams(),
                                      alloc.getShape());
  auto converted =
      fir::ConvertOp::create(builder, alloc.getLoc(), alloc.getRes().getType(),
                             alloca.getRes());

  mlir::MLIRContext *context = alloc.getContext();
  alloca->setAttr("fiap.rewrite_status",
                  mlir::StringAttr::get(context, "applied-stack-promotion"));
  alloca->setAttr("fiap.source_op",
                  mlir::StringAttr::get(context, "fir.allocmem"));
  converted->setAttr("fiap.rewrite_bridge",
                     mlir::StringAttr::get(context,
                                           "ref-to-heap-type-compatible-view"));

  llvm::SmallVector<fir::FreeMemOp> frees;
  for (mlir::Operation *user : alloc.getRes().getUsers()) {
    if (auto free = mlir::dyn_cast<fir::FreeMemOp>(user)) {
      frees.push_back(free);
    }
  }

  alloc.getRes().replaceAllUsesWith(converted.getRes());
  for (fir::FreeMemOp free : frees) {
    free.erase();
  }
  alloc.emitRemark("fiap promoted this compiler temporary from fir.allocmem to fir.alloca");
  alloc.erase();
  return true;
}
#endif

class PromoteTempToStackPass
    : public mlir::PassWrapper<PromoteTempToStackPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PromoteTempToStackPass)

  void runOnOperation() override {
    mlir::ModuleOp module = getOperation();
#if FIAP_HAVE_FLANG
    mlir::OpBuilder builder(module.getContext());
    llvm::SmallVector<fir::AllocMemOp> candidates;
    module.walk([&](fir::AllocMemOp alloc) { candidates.push_back(alloc); });

    unsigned promoted = 0;
    for (fir::AllocMemOp alloc : candidates) {
      if (!alloc->getBlock()) {
        continue;
      }
      if (promoteAllocMem(builder, alloc)) {
        ++promoted;
      }
    }
    if (promoted != 0) {
      module->setAttr("fiap.stack_promoted_allocmem",
                      mlir::IntegerAttr::get(
                          mlir::IntegerType::get(module.getContext(), 32),
                          promoted));
    }
#else
    module.walk([&](mlir::Operation *op) {
      if (!op->getName().getStringRef().contains("fir.allocmem")) {
        return;
      }

      if (!isStackPromotionCandidate(op)) {
        return;
      }

      mlir::MLIRContext *context = op->getContext();
      op->setAttr("fiap.lowering_hint",
                  mlir::StringAttr::get(context, "replace fir.allocmem with fir.alloca"));
      op->setAttr("fiap.rewrite_status",
                  mlir::StringAttr::get(context, "prepared-for-stack-promotion"));
      op->emitRemark("fiap prepared this allocation for stack promotion");
    });
#endif
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createPromoteTempToStackPass() {
  return std::make_unique<PromoteTempToStackPass>();
}

} // namespace fiap
