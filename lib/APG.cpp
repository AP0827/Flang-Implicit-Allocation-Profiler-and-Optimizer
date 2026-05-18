#include "fiap/APG.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Location.h"

#include <limits>
#include <utility>

using namespace fiap;

APGNode &AllocationProvenanceGraph::addNode(APGNode node) {
  node.id = nodes_.size();
  nodes_.push_back(std::move(node));
  if (nodes_.back().op) {
    opToNode_[nodes_.back().op] = nodes_.back().id;
  }
  return nodes_.back();
}

void AllocationProvenanceGraph::addEdge(std::size_t from, std::size_t to,
                                        APGEdgeKind kind) {
  edges_.push_back(APGEdge{from, to, kind});
}

APGNode *AllocationProvenanceGraph::lookupByOperation(mlir::Operation *op) {
  auto it = opToNode_.find(op);
  if (it == opToNode_.end()) {
    return nullptr;
  }
  return &nodes_[it->second];
}

const APGNode *
AllocationProvenanceGraph::lookupByOperation(mlir::Operation *op) const {
  auto it = opToNode_.find(op);
  if (it == opToNode_.end()) {
    return nullptr;
  }
  return &nodes_[it->second];
}

std::vector<const APGEdge *>
AllocationProvenanceGraph::incomingEdges(std::size_t nodeId) const {
  std::vector<const APGEdge *> incoming;
  for (const APGEdge &edge : edges_) {
    if (edge.to == nodeId) {
      incoming.push_back(&edge);
    }
  }
  return incoming;
}

std::vector<const APGEdge *>
AllocationProvenanceGraph::outgoingEdges(std::size_t nodeId) const {
  std::vector<const APGEdge *> outgoing;
  for (const APGEdge &edge : edges_) {
    if (edge.from == nodeId) {
      outgoing.push_back(&edge);
    }
  }
  return outgoing;
}

SourceAnchor fiap::describeLocation(mlir::Location location) {
  SourceAnchor anchor;

  if (auto fileLineCol = mlir::dyn_cast<mlir::FileLineColLoc>(location)) {
    anchor.file = fileLineCol.getFilename().str();
    anchor.line = fileLineCol.getLine();
    anchor.column = fileLineCol.getColumn();
    return anchor;
  }

  if (auto callsite = mlir::dyn_cast<mlir::CallSiteLoc>(location)) {
    return describeLocation(callsite.getCaller());
  }

  if (auto nameLoc = mlir::dyn_cast<mlir::NameLoc>(location)) {
    return describeLocation(nameLoc.getChildLoc());
  }

  if (auto fused = mlir::dyn_cast<mlir::FusedLoc>(location)) {
    for (mlir::Location child : fused.getLocations()) {
      SourceAnchor nested = describeLocation(child);
      if (nested.file != "<unknown>") {
        return nested;
      }
    }
  }

  anchor.file = "<unknown>";
  return anchor;
}

const char *fiap::toString(APGNodeKind value) {
  switch (value) {
  case APGNodeKind::Expression:
    return "expression";
  case APGNodeKind::Elemental:
    return "elemental";
  case APGNodeKind::Associate:
    return "associate";
  case APGNodeKind::AllocMem:
    return "allocmem";
  case APGNodeKind::Assign:
    return "assign";
  case APGNodeKind::Call:
    return "call";
  case APGNodeKind::Destroy:
    return "destroy";
  case APGNodeKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

const char *fiap::toString(APGEdgeKind value) {
  switch (value) {
  case APGEdgeKind::Produces:
    return "produces";
  case APGEdgeKind::Consumes:
    return "consumes";
  case APGEdgeKind::ShapeConstrains:
    return "shape-constrains";
  case APGEdgeKind::Aliases:
    return "aliases";
  case APGEdgeKind::LifetimeEnds:
    return "lifetime-ends";
  }
  return "unknown";
}

const char *fiap::toString(AllocationClass value) {
  switch (value) {
  case AllocationClass::ProvablyEliminable:
    return "provably-eliminable";
  case AllocationClass::PossiblyUnnecessary:
    return "possibly-unnecessary";
  case AllocationClass::Necessary:
    return "necessary";
  }
  return "unknown";
}

const char *fiap::toString(ImplicitConstructKind value) {
  switch (value) {
  case ImplicitConstructKind::ArrayExpressionTemporary:
    return "array-expression-temporary";
  case ImplicitConstructKind::ElementalTemporary:
    return "elemental-temporary";
  case ImplicitConstructKind::FunctionResultTemporary:
    return "function-result-temporary";
  case ImplicitConstructKind::ReallocOnAssignment:
    return "realloc-on-assignment";
  case ImplicitConstructKind::AssociateTemporary:
    return "associate-temporary";
  case ImplicitConstructKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

const char *fiap::toString(EscapeKind value) {
  switch (value) {
  case EscapeKind::NoEscape:
    return "no-escape";
  case EscapeKind::LocalEscape:
    return "local-escape";
  case EscapeKind::InterproceduralEscape:
    return "interprocedural-escape";
  }
  return "unknown";
}

const char *fiap::toString(TransformKind value) {
  switch (value) {
  case TransformKind::None:
    return "none";
  case TransformKind::PromoteToStack:
    return "promote-to-stack";
  case TransformKind::ScalarizeToLoopNest:
    return "scalarize-to-loop-nest";
  case TransformKind::PreallocateLHS:
    return "preallocate-lhs";
  case TransformKind::AddShapeGuard:
    return "add-shape-guard";
  }
  return "unknown";
}

bool fiap::isAllocationBearingNode(const APGNode &node) {
  if (node.kind == APGNodeKind::AllocMem) {
    return true;
  }
  if (node.kind == APGNodeKind::Assign) {
    return node.construct == ImplicitConstructKind::ReallocOnAssignment;
  }
  switch (node.construct) {
  case ImplicitConstructKind::ArrayExpressionTemporary:
  case ImplicitConstructKind::ElementalTemporary:
  case ImplicitConstructKind::FunctionResultTemporary:
  case ImplicitConstructKind::AssociateTemporary:
    return true;
  case ImplicitConstructKind::ReallocOnAssignment:
    return node.kind == APGNodeKind::Assign;
  case ImplicitConstructKind::Unknown:
    return false;
  }
  return false;
}

std::optional<std::uint64_t> fiap::estimateBytes(const ShapeInfo &shape) {
  if (shape.elementByteWidth == 0 || shape.extents.empty() || shape.hasDynamicExtent) {
    return std::nullopt;
  }

  std::uint64_t product = 1;
  for (std::int64_t extent : shape.extents) {
    if (extent <= 0) {
      return std::nullopt;
    }
    if (product > std::numeric_limits<std::uint64_t>::max() /
                      static_cast<std::uint64_t>(extent)) {
      return std::nullopt;
    }
    product *= static_cast<std::uint64_t>(extent);
  }
  return product * shape.elementByteWidth;
}
