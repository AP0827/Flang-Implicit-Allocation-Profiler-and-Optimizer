#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlir {
class Location;
class Operation;
} // namespace mlir

namespace fiap {

enum class APGNodeKind {
  Expression,
  Elemental,
  Associate,
  AllocMem,
  Assign,
  Call,
  Destroy,
  Unknown,
};

enum class APGEdgeKind {
  Produces,
  Consumes,
  ShapeConstrains,
  Aliases,
  LifetimeEnds,
};

enum class AllocationClass {
  ProvablyEliminable,
  PossiblyUnnecessary,
  Necessary,
};

enum class ImplicitConstructKind {
  ArrayExpressionTemporary,
  ElementalTemporary,
  FunctionResultTemporary,
  ReallocOnAssignment,
  AssociateTemporary,
  Unknown,
};

enum class EscapeKind {
  NoEscape,
  LocalEscape,
  InterproceduralEscape,
};

enum class TransformKind {
  None,
  PromoteToStack,
  ScalarizeToLoopNest,
  PreallocateLHS,
  AddShapeGuard,
};

struct SourceAnchor {
  std::string file;
  unsigned line = 0;
  unsigned column = 0;
};

struct ShapeInfo {
  std::vector<std::int64_t> extents;
  bool hasDynamicExtent = false;
  std::string typeSpelling;
  std::uint64_t elementByteWidth = 0;
};

struct AllocationEstimate {
  std::optional<std::uint64_t> elementCount;
  std::optional<std::uint64_t> byteCount;
};

struct APGNode {
  std::size_t id = 0;
  APGNodeKind kind = APGNodeKind::Unknown;
  ImplicitConstructKind construct = ImplicitConstructKind::Unknown;
  AllocationClass classification = AllocationClass::Necessary;
  EscapeKind escape = EscapeKind::NoEscape;
  TransformKind suggestedTransform = TransformKind::None;
  mlir::Operation *op = nullptr;
  SourceAnchor source;
  ShapeInfo shape;
  AllocationEstimate estimate;
  std::string opName;
  std::string summary;
  std::string reason;
  std::string advice;
  unsigned loopDepth = 0;
  unsigned consumerCount = 0;
  unsigned producerCount = 0;
  bool compilerGenerated = true;
  bool escapes = false;
  bool hasRuntimeDependentShape = false;
  bool transformable = false;
};

struct APGEdge {
  std::size_t from = 0;
  std::size_t to = 0;
  APGEdgeKind kind = APGEdgeKind::Produces;
};

class AllocationProvenanceGraph {
public:
  APGNode &addNode(APGNode node);
  void addEdge(std::size_t from, std::size_t to, APGEdgeKind kind);

  std::vector<APGNode> &nodes() { return nodes_; }
  const std::vector<APGNode> &nodes() const { return nodes_; }
  const std::vector<APGEdge> &edges() const { return edges_; }

  APGNode *lookupByOperation(mlir::Operation *op);
  const APGNode *lookupByOperation(mlir::Operation *op) const;
  std::vector<const APGEdge *> incomingEdges(std::size_t nodeId) const;
  std::vector<const APGEdge *> outgoingEdges(std::size_t nodeId) const;

private:
  std::vector<APGNode> nodes_;
  std::vector<APGEdge> edges_;
  std::unordered_map<mlir::Operation *, std::size_t> opToNode_;
};

SourceAnchor describeLocation(mlir::Location location);

const char *toString(APGNodeKind value);
const char *toString(APGEdgeKind value);
const char *toString(AllocationClass value);
const char *toString(ImplicitConstructKind value);
const char *toString(EscapeKind value);
const char *toString(TransformKind value);

bool isAllocationBearingNode(const APGNode &node);
std::optional<std::uint64_t> estimateBytes(const ShapeInfo &shape);

} // namespace fiap
