#pragma once

#include "fiap/AllocationReport.h"

namespace fiap {

struct AnalysisOptions {
  bool includeAssociates = true;
  bool includeAssignments = true;
  bool includeCalls = true;
  bool includeDestroyOps = true;
  bool includeUnknownOps = false;
};

struct ProfilerPassOptions {
  AnalysisOptions analysis;
  ReportFormat reportFormat = ReportFormat::Text;
  bool emitSummary = true;
  bool annotateIR = true;
  bool printAnnotatedIR = false;
  bool includeNonAllocationNodes = false;
};

} // namespace fiap
