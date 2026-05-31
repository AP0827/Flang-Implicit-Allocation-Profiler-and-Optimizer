#pragma once

#include "fiap/APG.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fiap {

enum class ReportFormat {
  Text,
  Json,
  Dot,
  ProfileSites,
  Sarif,
};

struct ReportEntry {
  std::size_t siteId = 0;
  SourceAnchor source;
  APGNodeKind nodeKind = APGNodeKind::Unknown;
  ImplicitConstructKind construct = ImplicitConstructKind::Unknown;
  AllocationClass classification = AllocationClass::Necessary;
  EscapeKind escape = EscapeKind::NoEscape;
  TransformKind suggestedTransform = TransformKind::None;
  std::string opName;
  std::string sourceLine;
  std::string sourceExpression;
  unsigned expressionColumn = 0;
  unsigned expressionLength = 0;
  std::string summary;
  std::string reason;
  std::string advice;
  unsigned loopDepth = 0;
  unsigned consumerCount = 0;
  unsigned producerCount = 0;
  unsigned rank = 0;
  std::string shapeExtents;
  std::uint64_t estimatedBytes = 0;
  std::uint64_t estimatedElements = 0;
  std::uint64_t elementByteWidth = 0;
  bool hasRuntimeDependentShape = false;
  bool assignmentCompatibleShape = false;
  bool transformable = false;
  bool aliasRisk = false;
  bool typedFlangMatch = false;
  std::string shapeEvidence;
  std::string aliasEvidence;
  std::string legality;
  std::string legalityReason;
};

struct ReportSummary {
  std::size_t totalSites = 0;
  std::size_t provablyEliminable = 0;
  std::size_t possiblyUnnecessary = 0;
  std::size_t necessary = 0;
  std::uint64_t totalEstimatedBytes = 0;
};

class AllocationReport {
public:
  static AllocationReport fromGraph(const AllocationProvenanceGraph &graph,
                                    bool includeNonAllocationNodes = false);

  const std::vector<ReportEntry> &entries() const { return entries_; }
  const ReportSummary &summary() const { return summary_; }

  std::string render(ReportFormat format,
                     const AllocationProvenanceGraph *graph = nullptr,
                     bool includeSummary = true) const;

private:
  std::string renderText(bool includeSummary) const;
  std::string renderJson(bool includeSummary) const;
  std::string renderSarif() const;
  std::string renderDot(const AllocationProvenanceGraph &graph) const;

  std::vector<ReportEntry> entries_;
  ReportSummary summary_;
};

const char *toString(ReportFormat value);

} // namespace fiap
