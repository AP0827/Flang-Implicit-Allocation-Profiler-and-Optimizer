#include "fiap/AllocationReport.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

using namespace fiap;

namespace {

std::string escapeJson(llvm::StringRef input) {
  std::string escaped;
  escaped.reserve(input.size() + 8);
  for (char c : input) {
    switch (c) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += c;
      break;
    }
  }
  return escaped;
}

} // namespace

AllocationReport
AllocationReport::fromGraph(const AllocationProvenanceGraph &graph,
                            bool includeNonAllocationNodes) {
  AllocationReport report;

  for (const APGNode &node : graph.nodes()) {
    if (!includeNonAllocationNodes && !isAllocationBearingNode(node)) {
      continue;
    }

    ReportEntry entry;
    entry.source = node.source;
    entry.nodeKind = node.kind;
    entry.construct = node.construct;
    entry.classification = node.classification;
    entry.escape = node.escape;
    entry.suggestedTransform = node.suggestedTransform;
    entry.opName = node.opName;
    entry.summary = node.summary;
    entry.reason = node.reason;
    entry.advice = node.advice;
    entry.loopDepth = node.loopDepth;
    entry.consumerCount = node.consumerCount;
    entry.producerCount = node.producerCount;
    entry.estimatedBytes = node.estimate.byteCount.value_or(0);
    entry.hasRuntimeDependentShape = node.hasRuntimeDependentShape;
    entry.transformable = node.transformable;
    report.entries_.push_back(std::move(entry));

    ++report.summary_.totalSites;
    switch (node.classification) {
    case AllocationClass::ProvablyEliminable:
      ++report.summary_.provablyEliminable;
      break;
    case AllocationClass::PossiblyUnnecessary:
      ++report.summary_.possiblyUnnecessary;
      break;
    case AllocationClass::Necessary:
      ++report.summary_.necessary;
      break;
    }
    report.summary_.totalEstimatedBytes += node.estimate.byteCount.value_or(0);
  }

  return report;
}

std::string AllocationReport::render(ReportFormat format,
                                     const AllocationProvenanceGraph *graph,
                                     bool includeSummary) const {
  switch (format) {
  case ReportFormat::Text:
    return renderText(includeSummary);
  case ReportFormat::Json:
    return renderJson(includeSummary);
  case ReportFormat::Dot:
    if (graph == nullptr) {
      return "digraph apg {}\n";
    }
    return renderDot(*graph);
  }
  return renderText(includeSummary);
}

std::string AllocationReport::renderText(bool includeSummary) const {
  std::string buffer;
  llvm::raw_string_ostream os(buffer);

  if (includeSummary) {
    os << "Summary: sites=" << summary_.totalSites
       << ", provably-eliminable=" << summary_.provablyEliminable
       << ", possibly-unnecessary=" << summary_.possiblyUnnecessary
       << ", necessary=" << summary_.necessary
       << ", estimated-bytes=" << summary_.totalEstimatedBytes << "\n";
  }

  for (const ReportEntry &entry : entries_) {
    os << entry.source.file << ":" << entry.source.line << ":" << entry.source.column
       << ": [" << toString(entry.classification) << "] " << entry.summary << "\n";
    os << "  op=" << entry.opName << " kind=" << toString(entry.nodeKind)
       << " construct=" << toString(entry.construct)
       << " escape=" << toString(entry.escape)
       << " loop-depth=" << entry.loopDepth
       << " consumers=" << entry.consumerCount
       << " producers=" << entry.producerCount << "\n";
    os << "  estimated-bytes=" << entry.estimatedBytes
       << " runtime-shape=" << (entry.hasRuntimeDependentShape ? "yes" : "no")
       << " transform=" << toString(entry.suggestedTransform) << "\n";
    os << "  reason=" << entry.reason << "\n";
    os << "  advice=" << entry.advice << "\n";
  }

  return os.str();
}

std::string AllocationReport::renderJson(bool includeSummary) const {
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  os << "{";

  if (includeSummary) {
    os << "\"summary\":{"
       << "\"totalSites\":" << summary_.totalSites << ","
       << "\"provablyEliminable\":" << summary_.provablyEliminable << ","
       << "\"possiblyUnnecessary\":" << summary_.possiblyUnnecessary << ","
       << "\"necessary\":" << summary_.necessary << ","
       << "\"totalEstimatedBytes\":" << summary_.totalEstimatedBytes << "},";
  }

  os << "\"entries\":[";
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const ReportEntry &entry = entries_[i];
    if (i != 0) {
      os << ",";
    }
    os << "{"
       << "\"file\":\"" << escapeJson(entry.source.file) << "\","
       << "\"line\":" << entry.source.line << ","
       << "\"column\":" << entry.source.column << ","
       << "\"classification\":\"" << toString(entry.classification) << "\","
       << "\"nodeKind\":\"" << toString(entry.nodeKind) << "\","
       << "\"construct\":\"" << toString(entry.construct) << "\","
       << "\"escape\":\"" << toString(entry.escape) << "\","
       << "\"transform\":\"" << toString(entry.suggestedTransform) << "\","
       << "\"opName\":\"" << escapeJson(entry.opName) << "\","
       << "\"summary\":\"" << escapeJson(entry.summary) << "\","
       << "\"reason\":\"" << escapeJson(entry.reason) << "\","
       << "\"advice\":\"" << escapeJson(entry.advice) << "\","
       << "\"loopDepth\":" << entry.loopDepth << ","
       << "\"consumerCount\":" << entry.consumerCount << ","
       << "\"producerCount\":" << entry.producerCount << ","
       << "\"estimatedBytes\":" << entry.estimatedBytes << ","
       << "\"runtimeDependentShape\":"
       << (entry.hasRuntimeDependentShape ? "true" : "false") << ","
       << "\"transformable\":" << (entry.transformable ? "true" : "false")
       << "}";
  }
  os << "]}";

  return os.str();
}

std::string AllocationReport::renderDot(const AllocationProvenanceGraph &graph) const {
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  os << "digraph apg {\n";
  os << "  rankdir=LR;\n";

  for (const APGNode &node : graph.nodes()) {
    os << "  n" << node.id << " [label=\""
       << node.id << " | " << toString(node.kind)
       << " | " << toString(node.classification)
       << " | " << toString(node.construct)
       << " | line " << node.source.line << "\"];\n";
  }

  for (const APGEdge &edge : graph.edges()) {
    os << "  n" << edge.from << " -> n" << edge.to
       << " [label=\"" << toString(edge.kind) << "\"];\n";
  }

  os << "}\n";
  return os.str();
}

const char *fiap::toString(ReportFormat value) {
  switch (value) {
  case ReportFormat::Text:
    return "text";
  case ReportFormat::Json:
    return "json";
  case ReportFormat::Dot:
    return "dot";
  }
  return "text";
}
