#include "fiap/AllocationReport.h"
#include "fiap/OperationSemantics.h"
#include "fiap/Passes.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/FileUtilities.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace {

fiap::ReportFormat parseReportFormat(const std::string &value) {
  if (value == "json") {
    return fiap::ReportFormat::Json;
  }
  if (value == "dot") {
    return fiap::ReportFormat::Dot;
  }
  if (value == "profile-sites") {
    return fiap::ReportFormat::ProfileSites;
  }
  if (value == "sarif") {
    return fiap::ReportFormat::Sarif;
  }
  return fiap::ReportFormat::Text;
}

} // namespace

int main(int argc, char **argv) {
  llvm::InitLLVM y(argc, argv);
  fiap::registerFIAPPasses();

  llvm::cl::opt<std::string> inputFilename(
      llvm::cl::Positional, llvm::cl::desc("<input MLIR file>"),
      llvm::cl::Required);
  llvm::cl::opt<std::string> format(
      "format", llvm::cl::desc("report output format"),
      llvm::cl::value_desc("text|json|dot|profile-sites|sarif"), llvm::cl::init("text"));
  llvm::cl::opt<bool> noSummary("no-summary",
                                llvm::cl::desc("suppress summary header"));
  llvm::cl::opt<bool> printAnnotatedIR(
      "print-annotated-ir",
      llvm::cl::desc("print the input module after fiap annotations are attached"));
  llvm::cl::opt<bool> prepareTransforms(
      "prepare-transforms",
      llvm::cl::desc("run prototype transformation-preparation passes after profiling"));
  llvm::cl::opt<bool> applyTransforms(
      "apply-transforms",
      llvm::cl::desc("run safe prototype transform passes after profiling"));
  llvm::cl::opt<bool> emitProfileSites(
      "emit-profile-sites",
      llvm::cl::desc("emit profile-site CSV rows instead of the selected report format"));
  llvm::cl::opt<bool> includeNonAllocationNodes(
      "include-non-allocation-nodes",
      llvm::cl::desc("include provenance-only nodes in the final report"));
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "fiap implicit allocation profiler\n");

  mlir::DialectRegistry registry;
  fiap::registerProjectDialects(registry);
  mlir::MLIRContext context(registry);
  context.allowUnregisteredDialects();
  context.loadAllAvailableDialects();

  llvm::SourceMgr sourceMgr;
  auto file = mlir::openInputFile(inputFilename);
  if (!file) {
    llvm::errs() << "unable to open input file: " << inputFilename << "\n";
    return 1;
  }

  sourceMgr.AddNewSourceBuffer(std::move(file), llvm::SMLoc());
  auto module = mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "unable to parse MLIR input\n";
    return 1;
  }

  fiap::ProfilerPassOptions options;
  options.reportFormat =
      emitProfileSites ? fiap::ReportFormat::ProfileSites : parseReportFormat(format);
  options.emitSummary = !noSummary;
  options.annotateIR = true;
  options.printAnnotatedIR = false;
  options.includeNonAllocationNodes = includeNonAllocationNodes;

  mlir::PassManager pm(&context);
  pm.addPass(fiap::createImplicitAllocationProfilerPass(options));
  if (prepareTransforms || applyTransforms) {
    pm.addPass(fiap::createPromoteTempToStackPass());
    pm.addPass(fiap::createScalarizeArrayExprPass());
  }

  if (mlir::failed(pm.run(*module))) {
    llvm::errs() << "pass pipeline failed\n";
    return 1;
  }

  if (printAnnotatedIR) {
    module->print(llvm::outs());
    llvm::outs() << "\n";
  }

  return 0;
}
