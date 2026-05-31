#include "fiap/ImplicitAllocationAnalysis.h"
#include "fiap/OperationSemantics.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/StringRef.h"

#include <sstream>
#include <unordered_set>
#include <vector>

using namespace fiap;

namespace {

unsigned computeLoopDepth(mlir::Operation *op) {
  unsigned depth = 0;
  for (mlir::Operation *parent = op->getParentOp(); parent != nullptr;
       parent = parent->getParentOp()) {
    const llvm::StringRef name = parent->getName().getStringRef();
    if (name.contains("fir.do_loop") || name.contains("scf.for") || name.contains("omp.wsloop")) {
      ++depth;
    }
  }
  return depth;
}

void connectDataflowEdges(AllocationProvenanceGraph &graph) {
  for (APGNode &node : graph.nodes()) {
    if (node.op == nullptr) {
      continue;
    }

    for (mlir::Value operand : node.op->getOperands()) {
      mlir::Operation *def = operand.getDefiningOp();
      if (def == nullptr) {
        continue;
      }
      APGNode *source = graph.lookupByOperation(def);
      if (source == nullptr) {
        continue;
      }

      if (node.kind == APGNodeKind::Destroy) {
        graph.addEdge(source->id, node.id, APGEdgeKind::LifetimeEnds);
        continue;
      }

      graph.addEdge(source->id, node.id, APGEdgeKind::Produces);
      if (node.kind == APGNodeKind::Assign || node.kind == APGNodeKind::Call ||
          node.kind == APGNodeKind::Destroy) {
        graph.addEdge(source->id, node.id, APGEdgeKind::Consumes);
      }
      if ((source->kind == APGNodeKind::Expression || source->kind == APGNodeKind::Elemental ||
           source->kind == APGNodeKind::Associate) &&
          (node.kind == APGNodeKind::AllocMem || node.kind == APGNodeKind::Assign)) {
        graph.addEdge(source->id, node.id, APGEdgeKind::ShapeConstrains);
      }
    }
  }
}

void connectAliasEdges(AllocationProvenanceGraph &graph) {
  for (APGNode &node : graph.nodes()) {
    if (node.op == nullptr) {
      continue;
    }

    for (mlir::Value result : node.op->getResults()) {
      std::vector<APGNode *> consumers;
      for (mlir::Operation *user : result.getUsers()) {
        APGNode *consumer = graph.lookupByOperation(user);
        if (consumer != nullptr && consumer->kind == APGNodeKind::Destroy) {
          continue;
        }
        if (consumer != nullptr) {
          consumers.push_back(consumer);
        }
      }

      if (consumers.size() > 1) {
        for (APGNode *consumer : consumers) {
          graph.addEdge(node.id, consumer->id, APGEdgeKind::Aliases);
        }
      }
    }
  }
}

void propagateConstructs(AllocationProvenanceGraph &graph) {
  for (APGNode &node : graph.nodes()) {
    if (node.construct != ImplicitConstructKind::Unknown) {
      continue;
    }

    for (const APGEdge *incoming : graph.incomingEdges(node.id)) {
      if (incoming->kind != APGEdgeKind::Produces &&
          incoming->kind != APGEdgeKind::ShapeConstrains) {
        continue;
      }
      const APGNode &source = graph.nodes()[incoming->from];
      if (source.construct != ImplicitConstructKind::Unknown) {
        node.construct = source.construct;
        break;
      }
    }
  }
}

void propagateAssignmentShapeEvidence(AllocationProvenanceGraph &graph) {
  for (APGNode &node : graph.nodes()) {
    if (!isAllocationBearingNode(node) || !node.hasRuntimeDependentShape) {
      continue;
    }

    for (const APGNode &candidate : graph.nodes()) {
      if (candidate.kind != APGNodeKind::Assign || !candidate.estimate.byteCount) {
        continue;
      }
      if (candidate.source.file != node.source.file ||
          candidate.source.line != node.source.line) {
        continue;
      }

      node.shape = candidate.shape;
      node.estimate = candidate.estimate;
      node.assignmentCompatibleShape = true;
      node.hasRuntimeDependentShape = false;
      node.shapeEvidence =
          "shape is assignment-compatible with the destination on the same source line";
      break;
    }
  }
}

std::string describeShapeEvidence(const APGNode &node) {
  if (node.shape.extents.empty()) {
    return "";
  }

  std::ostringstream os;
  os << (node.shape.hasDynamicExtent ? "runtime-dependent shape" : "static shape")
     << ": rank=" << node.shape.extents.size() << ", extents=";
  for (std::size_t i = 0; i < node.shape.extents.size(); ++i) {
    if (i != 0) {
      os << "x";
    }
    if (node.shape.extents[i] < 0) {
      os << "?";
    } else {
      os << node.shape.extents[i];
    }
  }
  if (node.shape.elementByteWidth != 0) {
    os << ", element-bytes=" << node.shape.elementByteWidth;
  }
  return os.str();
}

void deriveNodeFacts(AllocationProvenanceGraph &graph) {
  for (APGNode &node : graph.nodes()) {
    std::unordered_set<std::size_t> uniqueIncoming;
    for (const APGEdge *edge : graph.incomingEdges(node.id)) {
      if (edge->kind == APGEdgeKind::LifetimeEnds) {
        continue;
      }
      uniqueIncoming.insert(edge->from);
    }

    std::unordered_set<std::size_t> uniqueOutgoing;
    for (const APGEdge *edge : graph.outgoingEdges(node.id)) {
      if (edge->kind == APGEdgeKind::LifetimeEnds) {
        continue;
      }
      uniqueOutgoing.insert(edge->to);
    }

    node.producerCount = static_cast<unsigned>(uniqueIncoming.size());
    node.consumerCount = static_cast<unsigned>(uniqueOutgoing.size());
    node.hasRuntimeDependentShape =
        !node.assignmentCompatibleShape &&
        (node.shape.hasDynamicExtent || !node.estimate.byteCount.has_value());

    bool escapes = false;
    EscapeKind escapeKind = EscapeKind::NoEscape;
    if (node.op != nullptr) {
      for (mlir::Value result : node.op->getResults()) {
        for (mlir::Operation *user : result.getUsers()) {
          const llvm::StringRef userName = user->getName().getStringRef();
          if (userName.contains("fir.call") || userName.contains("func.call") ||
              userName.contains("func.return") || userName.contains("fir.result")) {
            escapes = true;
            escapeKind = EscapeKind::InterproceduralEscape;
          } else if (user != node.op && userName.contains("hlfir.associate")) {
            escapes = true;
            if (escapeKind == EscapeKind::NoEscape) {
              escapeKind = EscapeKind::LocalEscape;
            }
          }
        }
      }
    }
    node.escapes = escapes;
    node.escape = escapeKind;

    if (node.shapeEvidence.empty()) {
      node.shapeEvidence = describeShapeEvidence(node);
    }
  }
}

} // namespace

ImplicitAllocationAnalysis::ImplicitAllocationAnalysis(AnalysisOptions options)
    : options_(options) {}

AllocationProvenanceGraph
ImplicitAllocationAnalysis::build(mlir::ModuleOp module) const {
  AllocationProvenanceGraph graph;

  module.walk([&](mlir::Operation *op) {
    OperationSemantics semantics = classifyOperationSemantics(*op, options_);
    if (!semantics.interesting) {
      return;
    }

    APGNode node;
    node.op = op;
    node.opName = op->getName().getStringRef().str();
    node.kind = semantics.kind;
    node.construct = semantics.construct;
    node.source = describeLocation(op->getLoc());
    node.loopDepth = computeLoopDepth(op);
    node.compilerGenerated = semantics.compilerGenerated;
    node.typedFlangMatch = semantics.typedFlangMatch;
    node.shape = inferShapeInfo(*op);
    node.aliasRisk = hasConservativeAliasRisk(*op);
    node.aliasEvidence = describeAliasEvidence(*op);
    node.estimate.byteCount = estimateBytes(node.shape);
    if (node.shape.elementByteWidth != 0 && !node.shape.extents.empty() &&
        !node.shape.hasDynamicExtent && node.estimate.byteCount) {
      node.estimate.elementCount =
          *node.estimate.byteCount / node.shape.elementByteWidth;
    }
    node.summary = summarizeOperation(*op, semantics);
    graph.addNode(std::move(node));
  });

  connectDataflowEdges(graph);
  connectAliasEdges(graph);
  propagateConstructs(graph);
  deriveNodeFacts(graph);
  propagateAssignmentShapeEvidence(graph);
  deriveNodeFacts(graph);
  return graph;
}
