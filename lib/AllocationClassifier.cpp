#include "fiap/AllocationClassifier.h"

#include "llvm/ADT/StringRef.h"

#include <string>

using namespace fiap;

namespace {

bool hasIncomingAlias(const AllocationProvenanceGraph &graph, const APGNode &node) {
  for (const APGEdge *edge : graph.incomingEdges(node.id)) {
    if (edge->kind == APGEdgeKind::Aliases) {
      return true;
    }
  }
  for (const APGEdge *edge : graph.outgoingEdges(node.id)) {
    if (edge->kind == APGEdgeKind::Aliases) {
      return true;
    }
  }
  return false;
}

bool hasIncomingShapeConstraint(const AllocationProvenanceGraph &graph,
                                const APGNode &node) {
  for (const APGEdge *edge : graph.incomingEdges(node.id)) {
    if (edge->kind == APGEdgeKind::ShapeConstrains) {
      return true;
    }
  }
  return false;
}

void setLegality(APGNode &node, llvm::StringRef status,
                 llvm::StringRef reason) {
  node.legality = status.str();
  node.legalityReason = reason.str();
}

} // namespace

void AllocationClassifier::classify(AllocationProvenanceGraph &graph) const {
  for (APGNode &node : graph.nodes()) {
    classifyNode(graph, node);
  }
}

void AllocationClassifier::classifyNode(AllocationProvenanceGraph &graph,
                                        APGNode &node) const {
  if (!isAllocationBearingNode(node)) {
    node.classification = AllocationClass::Necessary;
    node.reason = "node participates in provenance tracking but is not itself an allocation-bearing site";
    node.advice = "focus optimization effort on connected allocation-bearing nodes";
    node.suggestedTransform = TransformKind::None;
    node.transformable = false;
    setLegality(node, "not-allocation-site",
                "provenance-only node is never rewritten directly");
    return;
  }

  const bool aliasingObserved = hasIncomingAlias(graph, node) || node.aliasRisk;
  const bool shapeConstrained = hasIncomingShapeConstraint(graph, node);
  const bool singleConsumer = node.consumerCount <= 1;
  const bool staticShape = !node.hasRuntimeDependentShape;
  const std::uint64_t bytes = node.estimate.byteCount.value_or(0);

  if (node.construct == ImplicitConstructKind::ReallocOnAssignment) {
    if (staticShape && singleConsumer && !aliasingObserved) {
      node.classification = AllocationClass::PossiblyUnnecessary;
      node.reason = "the assignment looks shape-stable, so repeated reallocations may be avoidable";
      node.advice = "preallocate the lhs once or add a shape guard before the assignment";
      node.suggestedTransform = TransformKind::PreallocateLHS;
      node.transformable = true;
      setLegality(node, "needs-runtime-guard",
                  "allocatable assignment rewrite needs allocation/shape checks");
      return;
    }

    node.classification = AllocationClass::PossiblyUnnecessary;
    node.reason = "automatic reallocation may be avoidable, but runtime shape equality is not proven";
    node.advice = "measure shape stability and add an explicit shape guard or preallocate the lhs";
    node.suggestedTransform = TransformKind::AddShapeGuard;
    node.transformable = true;
    setLegality(node, "needs-runtime-guard",
                "shape equality is not statically proven for automatic reallocation");
    return;
  }

  if (node.escapes || node.escape == EscapeKind::InterproceduralEscape) {
    node.classification = AllocationClass::Necessary;
    node.reason = "the temporary escapes the local statement or procedure, so local elimination is unsafe";
    node.advice = "look for caller-callee refactoring or explicit workspace passing";
    node.suggestedTransform = TransformKind::None;
    node.transformable = false;
    setLegality(node, "illegal-for-local-rewrite",
                "value escapes beyond the local statement/procedure");
    return;
  }

  if (aliasingObserved || !singleConsumer) {
    node.classification = AllocationClass::Necessary;
    node.reason = node.aliasRisk && !node.aliasEvidence.empty()
                      ? std::string("alias-sensitive storage prevents direct scalarization or stack promotion: ") +
                            node.aliasEvidence
                      : "multiple consumers or aliasing prevent direct scalarization or stack promotion";
    node.advice = "reduce aliasing first or split the expression so each temporary has one local consumer";
    node.suggestedTransform = TransformKind::None;
    node.transformable = false;
    setLegality(node, "illegal-for-local-rewrite",
                "multiple consumers or conservative alias evidence block local replacement");
    return;
  }

  if ((node.construct == ImplicitConstructKind::ArrayExpressionTemporary ||
       node.construct == ImplicitConstructKind::ElementalTemporary) &&
      singleConsumer && !aliasingObserved) {
    if (staticShape || shapeConstrained || node.assignmentCompatibleShape) {
      node.classification = AllocationClass::ProvablyEliminable;
      node.reason = node.assignmentCompatibleShape
                        ? "the temporary is single-consumer, non-escaping, and its shape is proven compatible with the assignment destination"
                        : "the temporary is single-consumer, non-escaping, and shaped by a nearby assignment";
      node.advice = "rewrite the statement into an explicit loop nest or lower directly into the destination";
      node.suggestedTransform = TransformKind::ScalarizeToLoopNest;
      node.transformable = true;
      setLegality(node, "legal-for-rewrite",
                  "single-consumer, non-escaping, alias-clean elemental/expression temporary");
      return;
    }

    node.classification = AllocationClass::PossiblyUnnecessary;
    node.reason = "the temporary is structurally local, but its runtime-dependent shape still needs validation";
    node.advice = "profile the shape and size at runtime, then specialize the hot stable cases";
    node.suggestedTransform = TransformKind::AddShapeGuard;
    node.transformable = true;
    setLegality(node, "needs-profile-evidence",
                "runtime-dependent shape needs profile or stronger static proof");
    return;
  }

  if (node.kind == APGNodeKind::AllocMem && staticShape && bytes != 0 && bytes <= 65536) {
    node.classification = AllocationClass::ProvablyEliminable;
    node.reason = "the allocation is statically bounded and small enough for stack promotion in a prototype lowering";
    node.advice = "replace heap materialization with stack storage or a compiler-local scratch buffer";
    node.suggestedTransform = TransformKind::PromoteToStack;
    node.transformable = true;
    setLegality(node, "legal-for-rewrite",
                "bounded non-escaping heap temporary is small enough for guarded stack promotion");
    return;
  }

  if (node.construct == ImplicitConstructKind::FunctionResultTemporary) {
    node.classification = AllocationClass::PossiblyUnnecessary;
    node.reason = node.assignmentCompatibleShape
                      ? "the destination shape is proven compatible, but removing a function-result temporary still needs an interprocedural result-buffer rewrite or profile guard"
                      : "array-valued function results often materialize a temporary, but proving elimination needs interprocedural shape reasoning";
    node.advice = node.assignmentCompatibleShape
                      ? "convert the function into a subroutine with an explicit result buffer, guarded by the proven destination shape"
                      : "consider converting the function into a subroutine with an explicit result buffer";
    node.suggestedTransform = TransformKind::PreallocateLHS;
    node.transformable = true;
    setLegality(node,
                node.assignmentCompatibleShape ? "needs-interprocedural-rewrite"
                                               : "needs-profile-evidence",
                node.assignmentCompatibleShape
                    ? "destination shape is known, but call/result ABI rewrite is still required"
                    : "array-valued result shape needs interprocedural or profile proof");
    return;
  }

  node.classification = AllocationClass::Necessary;
  node.reason = "the current analysis cannot prove that this allocation is removable";
  node.advice = "collect profile data or strengthen shape and alias analysis around this site";
  node.suggestedTransform = TransformKind::None;
  node.transformable = false;
  setLegality(node, "unproven",
              "current analysis did not find a sound local rewrite proof");
}
