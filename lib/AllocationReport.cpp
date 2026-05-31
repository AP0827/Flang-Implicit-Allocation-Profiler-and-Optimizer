#include "fiap/AllocationReport.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
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

std::string readSourceLine(const SourceAnchor &source) {
  if (source.file.empty() || source.line == 0) {
    return "";
  }

  std::ifstream input(source.file);
  if (!input) {
    return "";
  }

  std::string line;
  for (unsigned current = 1; std::getline(input, line); ++current) {
    if (current == source.line) {
      return line;
    }
  }
  return "";
}

struct SourceExpression {
  std::string text;
  unsigned column = 0;
  unsigned length = 0;
};

SourceExpression extractSourceExpression(llvm::StringRef sourceLine,
                                         const SourceAnchor &anchor) {
  SourceExpression expression;
  if (sourceLine.empty()) {
    return expression;
  }

  std::size_t equals = sourceLine.find('=');
  if (equals != llvm::StringRef::npos) {
    std::size_t start = equals + 1;
    while (start < sourceLine.size() &&
           std::isspace(static_cast<unsigned char>(sourceLine[start])) != 0) {
      ++start;
    }
    std::size_t end = sourceLine.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(sourceLine[end - 1])) != 0) {
      --end;
    }
    expression.text = sourceLine.substr(start, end - start).str();
    expression.column = static_cast<unsigned>(start + 1);
    expression.length = static_cast<unsigned>(end - start);
    return expression;
  }

  unsigned column = anchor.column == 0 ? 1 : anchor.column;
  std::size_t start = std::min<std::size_t>(column - 1, sourceLine.size());
  expression.text = sourceLine.substr(start).trim().str();
  expression.column = column;
  expression.length = static_cast<unsigned>(expression.text.size());
  return expression;
}

std::string formatExtents(const ShapeInfo &shape) {
  if (shape.extents.empty()) {
    return "";
  }
  std::string text;
  llvm::raw_string_ostream os(text);
  for (std::size_t i = 0; i < shape.extents.size(); ++i) {
    if (i != 0) {
      os << "x";
    }
    if (shape.extents[i] < 0) {
      os << "?";
    } else {
      os << shape.extents[i];
    }
  }
  return os.str();
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
    entry.siteId = node.id;
    entry.source = node.source;
    entry.nodeKind = node.kind;
    entry.construct = node.construct;
    entry.classification = node.classification;
    entry.escape = node.escape;
    entry.suggestedTransform = node.suggestedTransform;
    entry.opName = node.opName;
    entry.sourceLine = readSourceLine(node.source);
    SourceExpression sourceExpression =
        extractSourceExpression(entry.sourceLine, node.source);
    entry.sourceExpression = sourceExpression.text;
    entry.expressionColumn = sourceExpression.column;
    entry.expressionLength = sourceExpression.length;
    entry.summary = node.summary;
    entry.reason = node.reason;
    entry.advice = node.advice;
    entry.loopDepth = node.loopDepth;
    entry.consumerCount = node.consumerCount;
    entry.producerCount = node.producerCount;
    entry.rank = static_cast<unsigned>(node.shape.extents.size());
    entry.shapeExtents = formatExtents(node.shape);
    entry.estimatedBytes = node.estimate.byteCount.value_or(0);
    entry.estimatedElements = node.estimate.elementCount.value_or(0);
    entry.elementByteWidth = node.shape.elementByteWidth;
    entry.hasRuntimeDependentShape = node.hasRuntimeDependentShape;
    entry.assignmentCompatibleShape = node.assignmentCompatibleShape;
    entry.transformable = node.transformable;
    entry.aliasRisk = node.aliasRisk;
    entry.typedFlangMatch = node.typedFlangMatch;
    entry.shapeEvidence = node.shapeEvidence;
    entry.aliasEvidence = node.aliasEvidence;
    entry.legality = node.legality;
    entry.legalityReason = node.legalityReason;
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
  case ReportFormat::ProfileSites: {
    std::string buffer;
    llvm::raw_string_ostream os(buffer);
    os << "site_id,file,line,column,construct,classification,estimated_bytes,"
         "runtime_dependent_shape,assignment_compatible_shape,alias_risk,"
          "rank,estimated_elements,element_bytes,legality,transform\n";
    for (const ReportEntry &entry : entries_) {
      os << entry.siteId << ",\"" << escapeJson(entry.source.file) << "\","
         << entry.source.line << "," << entry.source.column << ","
         << toString(entry.construct) << "," << toString(entry.classification)
         << "," << entry.estimatedBytes << ","
         << (entry.hasRuntimeDependentShape ? "true" : "false") << ","
         << (entry.assignmentCompatibleShape ? "true" : "false") << ","
         << (entry.aliasRisk ? "true" : "false") << ","
         << entry.rank << "," << entry.estimatedElements << ","
         << entry.elementByteWidth << ","
         << entry.legality << ","
         << toString(entry.suggestedTransform) << "\n";
    }
    return os.str();
  }
  case ReportFormat::Sarif:
    return renderSarif();
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
    os << "#" << entry.siteId << " "
       << entry.source.file << ":" << entry.source.line << ":" << entry.source.column
       << ": [" << toString(entry.classification) << "] " << entry.summary << "\n";
    if (!entry.sourceLine.empty()) {
      os << "  source=\"" << entry.sourceLine << "\"\n";
    }
    if (!entry.sourceExpression.empty()) {
      os << "  expression=\"" << entry.sourceExpression
         << "\" column=" << entry.expressionColumn
         << " length=" << entry.expressionLength << "\n";
    }
    os << "  op=" << entry.opName << " kind=" << toString(entry.nodeKind)
       << " construct=" << toString(entry.construct)
       << " escape=" << toString(entry.escape)
       << " loop-depth=" << entry.loopDepth
       << " consumers=" << entry.consumerCount
       << " producers=" << entry.producerCount << "\n";
    os << "  estimated-bytes=" << entry.estimatedBytes
       << " estimated-elements=" << entry.estimatedElements
       << " element-bytes=" << entry.elementByteWidth
       << " rank=" << entry.rank
       << " extents=" << (entry.shapeExtents.empty() ? "unknown" : entry.shapeExtents)
       << " runtime-shape=" << (entry.hasRuntimeDependentShape ? "yes" : "no")
       << " assignment-shape=" << (entry.assignmentCompatibleShape ? "yes" : "no")
       << " alias-risk=" << (entry.aliasRisk ? "yes" : "no")
       << " typed-flang=" << (entry.typedFlangMatch ? "yes" : "no")
       << " transform=" << toString(entry.suggestedTransform) << "\n";
    if (!entry.legality.empty()) {
      os << "  legality=" << entry.legality;
      if (!entry.legalityReason.empty()) {
        os << " (" << entry.legalityReason << ")";
      }
      os << "\n";
    }
    if (!entry.shapeEvidence.empty()) {
      os << "  shape-evidence=" << entry.shapeEvidence << "\n";
    }
    if (!entry.aliasEvidence.empty()) {
      os << "  alias-evidence=" << entry.aliasEvidence << "\n";
    }
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
       << "\"siteId\":" << entry.siteId << ","
       << "\"file\":\"" << escapeJson(entry.source.file) << "\","
       << "\"line\":" << entry.source.line << ","
       << "\"column\":" << entry.source.column << ","
       << "\"classification\":\"" << toString(entry.classification) << "\","
       << "\"nodeKind\":\"" << toString(entry.nodeKind) << "\","
       << "\"construct\":\"" << toString(entry.construct) << "\","
       << "\"escape\":\"" << toString(entry.escape) << "\","
       << "\"transform\":\"" << toString(entry.suggestedTransform) << "\","
       << "\"opName\":\"" << escapeJson(entry.opName) << "\","
       << "\"sourceLine\":\"" << escapeJson(entry.sourceLine) << "\","
       << "\"sourceExpression\":\"" << escapeJson(entry.sourceExpression) << "\","
       << "\"expressionColumn\":" << entry.expressionColumn << ","
       << "\"expressionLength\":" << entry.expressionLength << ","
       << "\"summary\":\"" << escapeJson(entry.summary) << "\","
       << "\"reason\":\"" << escapeJson(entry.reason) << "\","
       << "\"advice\":\"" << escapeJson(entry.advice) << "\","
       << "\"loopDepth\":" << entry.loopDepth << ","
       << "\"consumerCount\":" << entry.consumerCount << ","
       << "\"producerCount\":" << entry.producerCount << ","
       << "\"rank\":" << entry.rank << ","
       << "\"shapeExtents\":\"" << escapeJson(entry.shapeExtents) << "\","
       << "\"estimatedBytes\":" << entry.estimatedBytes << ","
       << "\"estimatedElements\":" << entry.estimatedElements << ","
       << "\"elementByteWidth\":" << entry.elementByteWidth << ","
       << "\"runtimeDependentShape\":"
       << (entry.hasRuntimeDependentShape ? "true" : "false") << ","
       << "\"assignmentCompatibleShape\":"
       << (entry.assignmentCompatibleShape ? "true" : "false") << ","
       << "\"shapeEvidence\":\"" << escapeJson(entry.shapeEvidence) << "\","
       << "\"aliasRisk\":" << (entry.aliasRisk ? "true" : "false") << ","
       << "\"aliasEvidence\":\"" << escapeJson(entry.aliasEvidence) << "\","
       << "\"typedFlangMatch\":" << (entry.typedFlangMatch ? "true" : "false") << ","
       << "\"legality\":\"" << escapeJson(entry.legality) << "\","
       << "\"legalityReason\":\"" << escapeJson(entry.legalityReason) << "\","
       << "\"transformable\":" << (entry.transformable ? "true" : "false")
       << "}";
  }
  os << "]}";

  return os.str();
}

std::string AllocationReport::renderSarif() const {
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  os << "{"
     << "\"$schema\":\"https://json.schemastore.org/sarif-2.1.0.json\","
     << "\"version\":\"2.1.0\","
     << "\"runs\":[{"
     << "\"tool\":{\"driver\":{\"name\":\"FIAP\","
     << "\"informationUri\":\"https://github.com/llvm/llvm-project\","
     << "\"rules\":["
     << "{\"id\":\"FIAP001\",\"name\":\"implicit-allocation\","
     << "\"shortDescription\":{\"text\":\"Implicit HLFIR/FIR allocation site\"},"
     << "\"fullDescription\":{\"text\":\"FIAP detected a compiler-generated or allocation-relevant Fortran temporary.\"},"
     << "\"help\":{\"text\":\"Review classification, reason, and transformation advice in the message.\"}}"
     << "]}},"
     << "\"results\":[";

  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const ReportEntry &entry = entries_[i];
    if (i != 0) {
      os << ",";
    }
    const char *level = "note";
    if (entry.classification == AllocationClass::Necessary) {
      level = "warning";
    } else if (entry.classification == AllocationClass::ProvablyEliminable) {
      level = "none";
    }
    os << "{"
       << "\"ruleId\":\"FIAP001\","
       << "\"level\":\"" << level << "\","
       << "\"message\":{\"text\":\""
       << escapeJson(entry.summary) << " | classification="
       << toString(entry.classification) << " | estimated-bytes="
       << entry.estimatedBytes << " | transform="
       << toString(entry.suggestedTransform) << " | legality="
       << escapeJson(entry.legality) << " | advice="
       << escapeJson(entry.advice) << "\"},"
       << "\"locations\":[{\"physicalLocation\":{\"artifactLocation\":{\"uri\":\""
       << escapeJson(entry.source.file) << "\"},"
       << "\"region\":{\"startLine\":" << entry.source.line
       << ",\"startColumn\":" << (entry.expressionColumn == 0 ? entry.source.column : entry.expressionColumn)
       << ",\"charLength\":" << entry.expressionLength
       << ",\"snippet\":{\"text\":\"" << escapeJson(entry.sourceLine) << "\"}}}}],"
       << "\"properties\":{"
       << "\"siteId\":" << entry.siteId << ","
       << "\"nodeKind\":\"" << toString(entry.nodeKind) << "\","
       << "\"construct\":\"" << toString(entry.construct) << "\","
       << "\"escape\":\"" << toString(entry.escape) << "\","
       << "\"rank\":" << entry.rank << ","
       << "\"shapeExtents\":\"" << escapeJson(entry.shapeExtents) << "\","
       << "\"estimatedElements\":" << entry.estimatedElements << ","
       << "\"elementByteWidth\":" << entry.elementByteWidth << ","
       << "\"aliasRisk\":" << (entry.aliasRisk ? "true" : "false") << ","
       << "\"typedFlangMatch\":" << (entry.typedFlangMatch ? "true" : "false") << ","
       << "\"legality\":\"" << escapeJson(entry.legality) << "\","
       << "\"legalityReason\":\"" << escapeJson(entry.legalityReason) << "\","
       << "\"shapeEvidence\":\"" << escapeJson(entry.shapeEvidence) << "\","
       << "\"aliasEvidence\":\"" << escapeJson(entry.aliasEvidence) << "\","
       << "\"reason\":\"" << escapeJson(entry.reason) << "\""
       << "}"
       << "}";
  }

  os << "]}]}";
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
  case ReportFormat::ProfileSites:
    return "profile-sites";
  case ReportFormat::Sarif:
    return "sarif";
  }
  return "text";
}
