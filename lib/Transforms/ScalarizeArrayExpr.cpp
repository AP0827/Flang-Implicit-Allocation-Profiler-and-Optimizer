#include "fiap/Passes.h"

#include "fiap/Config.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <optional>

#if FIAP_HAVE_FLANG
#include "flang/Optimizer/Builder/FIRBuilder.h"
#include "flang/Optimizer/Builder/HLFIRTools.h"
#include "flang/Optimizer/HLFIR/HLFIROps.h"
#endif

namespace fiap {
namespace {

#if FIAP_HAVE_FLANG
struct ScalarizationPlan {
  hlfir::ElementalOp elemental;
  hlfir::AssignOp assign;
  llvm::SmallVector<hlfir::DestroyOp> destroys;
};
#endif

bool hasStringAttr(mlir::Operation *op, llvm::StringRef name,
                   llvm::StringRef expected) {
  auto attr = op->getAttrOfType<mlir::StringAttr>(name);
  return attr && attr.getValue() == expected;
}

bool isScalarizationCandidate(mlir::Operation *op) {
  return hasStringAttr(op, "fiap.classification", "provably-eliminable") &&
         hasStringAttr(op, "fiap.transform", "scalarize-to-loop-nest") &&
         hasStringAttr(op, "fiap.legality", "legal-for-rewrite");
}

#if FIAP_HAVE_FLANG
std::optional<ScalarizationPlan>
buildScalarizationPlan(hlfir::ElementalOp elemental) {
  if (!isScalarizationCandidate(elemental.getOperation())) {
    return std::nullopt;
  }

  auto resultType = mlir::dyn_cast<hlfir::ExprType>(elemental.getResult().getType());
  if (!resultType || resultType.getRank() < 1 || resultType.getRank() > 15) {
    elemental->setAttr(
        "fiap.rewrite_status",
        mlir::StringAttr::get(elemental.getContext(),
                              "skipped-invalid-fortran-rank"));
    return std::nullopt;
  }

  ScalarizationPlan plan;
  plan.elemental = elemental;
  mlir::Value result = elemental.getResult();

  for (mlir::Operation *user : result.getUsers()) {
    if (auto assign = mlir::dyn_cast<hlfir::AssignOp>(user)) {
      if (assign.getRhs() != result || plan.assign) {
        return std::nullopt;
      }
      if (assign.getRealloc()) {
        elemental->setAttr("fiap.rewrite_status",
                           mlir::StringAttr::get(elemental.getContext(),
                                                 "skipped-realloc-assign"));
        return std::nullopt;
      }
      if (!hlfir::isFortranEntity(assign.getLhs())) {
        elemental->setAttr("fiap.rewrite_status",
                           mlir::StringAttr::get(elemental.getContext(),
                                                 "skipped-non-fortran-lhs"));
        return std::nullopt;
      }
      plan.assign = assign;
      continue;
    }

    if (auto destroy = mlir::dyn_cast<hlfir::DestroyOp>(user)) {
      if (destroy.getExpr() != result) {
        return std::nullopt;
      }
      plan.destroys.push_back(destroy);
      continue;
    }

    elemental->setAttr("fiap.rewrite_status",
                       mlir::StringAttr::get(elemental.getContext(),
                                             "skipped-shared-expression"));
    return std::nullopt;
  }

  if (!plan.assign) {
    return std::nullopt;
  }
  return plan;
}

bool hasOnlyDestroyUsers(hlfir::ElementalOp elemental,
                         llvm::SmallVectorImpl<hlfir::DestroyOp> &destroys) {
  for (mlir::Operation *user : elemental.getResult().getUsers()) {
    auto destroy = mlir::dyn_cast<hlfir::DestroyOp>(user);
    if (!destroy || destroy.getExpr() != elemental.getResult()) {
      return false;
    }
    destroys.push_back(destroy);
  }
  return true;
}

void eraseDeadElementals(mlir::ModuleOp module) {
  while (true) {
    llvm::SmallVector<hlfir::ElementalOp> elementals;
    module.walk([&](hlfir::ElementalOp elemental) {
      elementals.push_back(elemental);
    });

    bool erasedOne = false;
    for (hlfir::ElementalOp elemental : elementals) {
      llvm::SmallVector<hlfir::DestroyOp> destroys;
      if (!hasOnlyDestroyUsers(elemental, destroys)) {
        continue;
      }
      for (hlfir::DestroyOp destroy : destroys) {
        destroy.erase();
      }
      elemental.erase();
      erasedOne = true;
      break;
    }

    if (!erasedOne) {
      return;
    }
  }
}

bool applyScalarization(fir::FirOpBuilder &builder,
                        const ScalarizationPlan &plan) {
  hlfir::ElementalOp elemental = plan.elemental;
  hlfir::AssignOp assign = plan.assign;
  mlir::Location loc = assign.getLoc();

  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(assign);

  hlfir::LoopNest loopNest =
      hlfir::genLoopNest(loc, builder, elemental.getShape(),
                         /*isUnordered=*/!elemental.isOrdered(),
                         /*emitWorkshareLoop=*/false);
  if (loopNest.outerOp) {
    mlir::MLIRContext *context = elemental.getContext();
    loopNest.outerOp->setAttr(
        "fiap.rewrite_status",
        mlir::StringAttr::get(context, "applied-scalarization"));
    loopNest.outerOp->setAttr("fiap.rewrite_template",
                              mlir::StringAttr::get(
                                  context, "explicit fir.do_loop nest"));
    loopNest.outerOp->setAttr("fiap.source_op",
                              mlir::StringAttr::get(context,
                                                    "hlfir.elemental"));
  }

  builder.setInsertionPointToStart(loopNest.body);
  mlir::IRMapping mapper;
  mlir::Value rhsElement = hlfir::inlineElementalOp(
      loc, builder, mlir::cast<hlfir::ElementalOpInterface>(*elemental),
      loopNest.oneBasedIndices, mapper,
      [](hlfir::ElementalOp nestedElemental) {
        return isScalarizationCandidate(nestedElemental.getOperation());
      });
  hlfir::Entity lhs(assign.getLhs());
  hlfir::Entity lhsElement =
      hlfir::getElementAt(loc, builder, lhs, loopNest.oneBasedIndices);
  hlfir::AssignOp::create(builder, loc, rhsElement, lhsElement,
                          /*realloc=*/false,
                          /*keep_lhs_length_if_realloc=*/false,
                          /*temporary_lhs=*/false);

  elemental.emitRemark("fiap replaced this hlfir.elemental assignment with an explicit loop nest");
  assign.erase();
  for (hlfir::DestroyOp destroy : plan.destroys) {
    destroy.erase();
  }
  elemental.erase();
  return true;
}
#endif

class ScalarizeArrayExprPass
    : public mlir::PassWrapper<ScalarizeArrayExprPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ScalarizeArrayExprPass)

  void runOnOperation() override {
    mlir::ModuleOp module = getOperation();
#if FIAP_HAVE_FLANG
    mlir::OpBuilder opBuilder(module.getContext());
    fir::FirOpBuilder builder(opBuilder, module);
    llvm::SmallVector<ScalarizationPlan> plans;

    module.walk([&](hlfir::ElementalOp elemental) {
      if (auto plan = buildScalarizationPlan(elemental)) {
        plans.push_back(*plan);
      }
    });

    unsigned rewritten = 0;
    for (const ScalarizationPlan &plan : plans) {
      if (!plan.elemental->getBlock() || !plan.assign->getBlock()) {
        continue;
      }
      if (applyScalarization(builder, plan)) {
        ++rewritten;
      }
    }
    eraseDeadElementals(module);

    if (rewritten != 0) {
      module->setAttr("fiap.scalarized_elementals",
                      mlir::IntegerAttr::get(mlir::IntegerType::get(module.getContext(), 32),
                                             rewritten));
    }
#else
    module.walk([&](mlir::Operation *op) {
      if (!isScalarizationCandidate(op)) {
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
#endif
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createScalarizeArrayExprPass() {
  return std::make_unique<ScalarizeArrayExprPass>();
}

} // namespace fiap
