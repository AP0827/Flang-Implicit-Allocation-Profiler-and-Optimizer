#pragma once

#include "fiap/APG.h"

namespace fiap {

class AllocationClassifier {
public:
  void classify(AllocationProvenanceGraph &graph) const;

private:
  void classifyNode(AllocationProvenanceGraph &graph, APGNode &node) const;
};

} // namespace fiap
