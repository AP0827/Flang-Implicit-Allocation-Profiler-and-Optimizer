#include "fiap/OperationSemantics.h"

#include "fiap\Config.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#if FIAP_HAVE_FLANG
#include "flang/Optimizer/Dialect/FIRDialect.h"
#include "flang/Optimizer/Dialect/FIROps.h"
#include "flang/Optimizer/Dialect/FIRType.h"
#include "flang/Optimizer/HLFIR/HLFIRDialect.h"
#include "flang/Optimizer/HLFIR/HLFIROps.h"
#include "flang/Optimizer/Support/InitFIR.h"
#endif

using namespace fiap;

namespace {

std::string printType(mlir::Type type) {
  std::string text;
  llvm::raw_string_ostream os(text);
  os << type;
  return os.str();
}

std::string printAttribute(mlir::Attribute attr) {
  std::string text;
  llvm::raw_string_ostream os(text);
  os << attr;
  return os.str();
}

bool isNumericToken(llvm::StringRef token) {
  return !token.empty() &&
         llvm::all_of(token, [](char c) {
           return std::isdigit(static_cast<unsigned char>(c)) != 0;
         });
}

std::vector<std::string> splitTopLevelX(llvm::StringRef body) {
  std::vector<std::string> tokens;
  std::string current;
  int nesting = 0;
  for (char c : body) {
    if (c == '<') {
      ++nesting;
      current.push_back(c);
      continue;
    }
    if (c == '>') {
      --nesting;
      current.push_back(c);
      continue;
    }
    if (c == 'x' && nesting == 0) {
      tokens.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

std::optional<std::string> extractArrayBody(llvm::StringRef typeSpelling) {
  const llvm::StringRef markers[] = {"!fir.array<", "!hlfir.expr<",
                                     "!fir.box<!fir.array<"};
  for (llvm::StringRef marker : markers) {
    std::size_t markerPos = typeSpelling.find(marker);
    if (markerPos == llvm::StringRef::npos) {
      continue;
    }

    std::size_t bodyStart = markerPos + marker.size();
    int depth = 1;
    for (std::size_t i = bodyStart; i < typeSpelling.size(); ++i) {
      if (typeSpelling[i] == '<') {
        ++depth;
      } else if (typeSpelling[i] == '>') {
        --depth;
        if (depth == 0) {
          return typeSpelling.substr(bodyStart, i - bodyStart).str();
        }
      }
    }
  }
  return std::nullopt;
}

std::uint64_t parseElementByteWidth(llvm::StringRef token) {
  if (token.consume_front("complex<")) {
    token = token.drop_back();
    std::uint64_t inner = parseElementByteWidth(token);
    return inner == 0 ? 0 : inner * 2;
  }

  if (token == "bf16" || token == "f16") {
    return 2;
  }
  if (token == "f32" || token == "i32") {
    return 4;
  }
  if (token == "f64" || token == "i64") {
    return 8;
  }
  if (token == "f80") {
    return 10;
  }
  if (token == "f128") {
    return 16;
  }
  if (token == "i8" || token == "logical<1>" || token == "character") {
    return 1;
  }
  if ((token.starts_with("i") || token.starts_with("f")) && token.size() > 1 &&
      isNumericToken(token.drop_front())) {
    return static_cast<std::uint64_t>(std::stoull(token.drop_front().str()) / 8);
  }

  return 0;
}

std::uint64_t inferElementByteWidthFromType(mlir::Type type) {
  if (!type) {
    return 0;
  }

  if (type.isIntOrFloat()) {
    return type.getIntOrFloatBitWidth() / 8;
  }

  if (auto complexType = mlir::dyn_cast<mlir::ComplexType>(type)) {
    mlir::Type elementType = complexType.getElementType();
    return elementType.isIntOrFloat() ? (elementType.getIntOrFloatBitWidth() / 8) * 2
                                      : 0;
  }

  return parseElementByteWidth(printType(type));
}

bool hasAttrContaining(mlir::Operation &op, llvm::StringRef needle) {
  for (mlir::NamedAttribute attr : op.getAttrs()) {
    if (attr.getName().strref().contains(needle) ||
        printAttribute(attr.getValue()).find(needle.str()) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool genericReturnsArrayLike(mlir::Operation &op) {
  for (mlir::Type type : op.getResultTypes()) {
    const std::string printed = printType(type);
    if (printed.find("!fir.array<") != std::string::npos ||
        printed.find("!hlfir.expr<") != std::string::npos ||
        printed.find("!fir.box<!fir.array<") != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool typeSpellingHasAliasRisk(mlir::Type type) {
  const std::string printed = printType(type);
  return printed.find("!fir.ptr<") != std::string::npos ||
         printed.find("!fir.box<") != std::string::npos ||
         printed.find("!fir.class<") != std::string::npos ||
         printed.find("!fir.ref<!fir.box<") != std::string::npos;
}

bool attrSpellingHasAliasRisk(mlir::Operation &op) {
  for (mlir::NamedAttribute attr : op.getAttrs()) {
    const std::string name = attr.getName().str();
    const std::string value = printAttribute(attr.getValue());
    if (name.find("pointer") != std::string::npos ||
        name.find("target") != std::string::npos ||
        value.find("pointer") != std::string::npos ||
        value.find("target") != std::string::npos ||
        value.find("intent_inout") != std::string::npos ||
        value.find("intent_out") != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool operationHasDirectAliasRisk(mlir::Operation &op) {
  if (attrSpellingHasAliasRisk(op)) {
    return true;
  }
  for (mlir::Type type : op.getResultTypes()) {
    if (typeSpellingHasAliasRisk(type)) {
      return true;
    }
  }
  for (mlir::Value operand : op.getOperands()) {
    if (typeSpellingHasAliasRisk(operand.getType())) {
      return true;
    }
  }
  return false;
}

ShapeInfo inferShapeFromTypesGeneric(mlir::Operation &op,
                                     bool preferDestinationOperand = false) {
  ShapeInfo shape;

  auto inspectType = [&](mlir::Type type) -> bool {
    shape.typeSpelling = printType(type);
    std::optional<std::string> body = extractArrayBody(shape.typeSpelling);
    if (!body) {
      return false;
    }

    std::vector<std::string> tokens = splitTopLevelX(*body);
    if (tokens.empty()) {
      return false;
    }

    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
      llvm::StringRef token(tokens[i]);
      if (token == "?") {
        shape.extents.push_back(-1);
        shape.hasDynamicExtent = true;
        continue;
      }
      if (!isNumericToken(token)) {
        return false;
      }
      shape.extents.push_back(static_cast<std::int64_t>(std::stoll(token.str())));
    }

    shape.elementByteWidth = parseElementByteWidth(tokens.back());
    return true;
  };

  if (!preferDestinationOperand) {
    for (mlir::Type type : op.getResultTypes()) {
      if (inspectType(type)) {
        return shape;
      }
    }
  }

  if (preferDestinationOperand) {
    for (mlir::Value operand : llvm::reverse(op.getOperands())) {
      if (inspectType(operand.getType())) {
        return shape;
      }
    }
  } else {
    for (mlir::Value operand : op.getOperands()) {
      if (inspectType(operand.getType())) {
        return shape;
      }
    }
  }

  if (preferDestinationOperand) {
    for (mlir::Type type : op.getResultTypes()) {
      if (inspectType(type)) {
        return shape;
      }
    }
  }
  return shape;
}

OperationSemantics classifyGeneric(mlir::Operation &op,
                                   const AnalysisOptions &options) {
  OperationSemantics semantics;
  const llvm::StringRef name = op.getName().getStringRef();

  if (name.contains("hlfir.as_expr")) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Expression;
    semantics.construct = ImplicitConstructKind::ArrayExpressionTemporary;
  } else if (name.contains("hlfir.elemental")) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Elemental;
    semantics.construct = ImplicitConstructKind::ElementalTemporary;
  } else if (options.includeAssociates && name.contains("hlfir.associate")) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Associate;
    semantics.construct = ImplicitConstructKind::AssociateTemporary;
  } else if (name.contains("fir.allocmem")) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::AllocMem;
  } else if (options.includeAssignments &&
             (name.contains("hlfir.assign") || name.contains("fir.store") ||
              hasAttrContaining(op, "realloc"))) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Assign;
    if (hasAttrContaining(op, "realloc")) {
      semantics.construct = ImplicitConstructKind::ReallocOnAssignment;
    }
  } else if (options.includeCalls &&
             (name.contains("fir.call") || name.contains("func.call"))) {
    semantics.returnsArrayLike = genericReturnsArrayLike(op);
    if (semantics.returnsArrayLike) {
      semantics.interesting = true;
      semantics.kind = APGNodeKind::Call;
      semantics.construct = ImplicitConstructKind::FunctionResultTemporary;
    }
  } else if (options.includeDestroyOps &&
             (name.contains("hlfir.destroy") || name.contains("fir.freemem"))) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Destroy;
  } else if (options.includeUnknownOps) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Unknown;
  }

  semantics.compilerGenerated =
      name.contains("hlfir.") || name.contains("fir.");
  if (!semantics.returnsArrayLike) {
    semantics.returnsArrayLike = genericReturnsArrayLike(op);
  }
  return semantics;
}

#if FIAP_HAVE_FLANG

bool fillShapeFromTypedType(mlir::Type type, ShapeInfo &shape) {
  shape.typeSpelling = printType(type);

  mlir::Type entityType = fir::unwrapRefType(type);
  entityType = fir::unwrapPassByRefType(entityType);

  if (auto exprType = mlir::dyn_cast<hlfir::ExprType>(entityType)) {
    for (auto extent : exprType.getShape()) {
      if (extent == fir::SequenceType::getUnknownExtent()) {
        shape.extents.push_back(-1);
        shape.hasDynamicExtent = true;
      } else {
        shape.extents.push_back(extent);
      }
    }
    shape.elementByteWidth =
        inferElementByteWidthFromType(hlfir::getFortranElementType(exprType.getEleTy()));
    return exprType.isArray();
  }

  if (auto boxType = mlir::dyn_cast<fir::BaseBoxType>(entityType)) {
    entityType = boxType.unwrapInnerType();
  } else if (auto ptrEleType = fir::dyn_cast_ptrEleTy(entityType)) {
    entityType = ptrEleType;
  }

  if (auto seqType = mlir::dyn_cast<fir::SequenceType>(entityType)) {
    for (auto extent : seqType.getShape()) {
      if (extent == fir::SequenceType::getUnknownExtent()) {
        shape.extents.push_back(-1);
        shape.hasDynamicExtent = true;
      } else {
        shape.extents.push_back(extent);
      }
    }
    shape.elementByteWidth = inferElementByteWidthFromType(seqType.getEleTy());
    return true;
  }

  return false;
}

bool typedReturnsArrayLike(mlir::Operation &op) {
  for (mlir::Type type : op.getResultTypes()) {
    mlir::Type unwrapped = fir::unwrapRefType(type);
    unwrapped = fir::unwrapPassByRefType(unwrapped);
    if (auto exprType = mlir::dyn_cast<hlfir::ExprType>(unwrapped)) {
      if (exprType.isArray()) {
        return true;
      }
    }
    if (mlir::isa<fir::SequenceType>(unwrapped) ||
        mlir::isa<fir::BaseBoxType>(unwrapped)) {
      return true;
    }
    if (auto ptrEleType = fir::dyn_cast_ptrEleTy(unwrapped)) {
      if (mlir::isa<fir::SequenceType>(ptrEleType)) {
        return true;
      }
    }
  }
  return false;
}

OperationSemantics classifyTyped(mlir::Operation &op,
                                 const AnalysisOptions &options) {
  OperationSemantics semantics;
  bool matchedTypedKind = false;

  if (mlir::isa<hlfir::AsExprOp>(op)) {
    matchedTypedKind = true;
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Expression;
    semantics.construct = ImplicitConstructKind::ArrayExpressionTemporary;
  } else if (mlir::isa<hlfir::ElementalOp>(op)) {
    matchedTypedKind = true;
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Elemental;
    semantics.construct = ImplicitConstructKind::ElementalTemporary;
  } else if (options.includeAssociates && mlir::isa<hlfir::AssociateOp>(op)) {
    matchedTypedKind = true;
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Associate;
    semantics.construct = ImplicitConstructKind::AssociateTemporary;
  } else if (mlir::isa<fir::AllocMemOp>(op)) {
    matchedTypedKind = true;
    semantics.interesting = true;
    semantics.kind = APGNodeKind::AllocMem;
  } else if (options.includeAssignments &&
             (mlir::isa<hlfir::AssignOp>(op) || hasAttrContaining(op, "realloc"))) {
    matchedTypedKind = mlir::isa<hlfir::AssignOp>(op);
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Assign;
    if (hasAttrContaining(op, "realloc")) {
      semantics.construct = ImplicitConstructKind::ReallocOnAssignment;
    }
  } else if (options.includeCalls &&
             (mlir::isa<fir::CallOp>(op) || mlir::isa<mlir::func::CallOp>(op))) {
    matchedTypedKind = mlir::isa<fir::CallOp>(op);
    semantics.returnsArrayLike = typedReturnsArrayLike(op);
    if (semantics.returnsArrayLike) {
      semantics.interesting = true;
      semantics.kind = APGNodeKind::Call;
      semantics.construct = ImplicitConstructKind::FunctionResultTemporary;
    }
  } else if (options.includeDestroyOps &&
             (mlir::isa<hlfir::DestroyOp>(op) || mlir::isa<fir::FreeMemOp>(op))) {
    matchedTypedKind = true;
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Destroy;
  } else if (options.includeUnknownOps) {
    semantics.interesting = true;
    semantics.kind = APGNodeKind::Unknown;
  }

  semantics.compilerGenerated =
      op.getName().getStringRef().contains("hlfir.") ||
      op.getName().getStringRef().contains("fir.");
  semantics.typedFlangMatch = matchedTypedKind;
  if (!semantics.returnsArrayLike) {
    semantics.returnsArrayLike = typedReturnsArrayLike(op);
  }
  return semantics;
}

#endif

} // namespace

bool fiap::compiledWithFlangSupport() { return FIAP_HAVE_FLANG != 0; }

void fiap::registerProjectDialects(mlir::DialectRegistry &registry) {
  registry.insert<mlir::func::FuncDialect>();
#if FIAP_HAVE_FLANG
  fir::support::registerNonCodegenDialects(registry);
  // FIAP analyzes FIR/HLFIR allocation operations only. Optional FIR extension
  // hooks pull OpenMP/OpenACC support libraries into the link and are not
  // needed for this standalone reporting tool.
#endif
}

OperationSemantics
fiap::classifyOperationSemantics(mlir::Operation &op,
                                 const AnalysisOptions &options) {
#if FIAP_HAVE_FLANG
  OperationSemantics typed = classifyTyped(op, options);
  if (typed.interesting) {
    return typed;
  }
#endif
  return classifyGeneric(op, options);
}

bool fiap::hasConservativeAliasRisk(mlir::Operation &op) {
  if (operationHasDirectAliasRisk(op)) {
    return true;
  }
  bool nestedRisk = false;
  op.walk([&](mlir::Operation *nested) {
    if (nested == &op || nestedRisk) {
      return;
    }
    nestedRisk = operationHasDirectAliasRisk(*nested);
  });
  return nestedRisk;
}

std::string fiap::describeAliasEvidence(mlir::Operation &op) {
  if (attrSpellingHasAliasRisk(op)) {
    return "operation attributes mention pointer/target/intent-out alias-sensitive Fortran semantics";
  }
  for (mlir::Type type : op.getResultTypes()) {
    if (typeSpellingHasAliasRisk(type)) {
      return "result type is descriptor/pointer/class-like and may alias external storage";
    }
  }
  for (mlir::Value operand : op.getOperands()) {
    if (typeSpellingHasAliasRisk(operand.getType())) {
      return "operand type is descriptor/pointer/class-like and may alias external storage";
    }
  }
  std::string nestedEvidence;
  op.walk([&](mlir::Operation *nested) {
    if (nested == &op || !nestedEvidence.empty()) {
      return;
    }
    if (operationHasDirectAliasRisk(*nested)) {
      nestedEvidence =
          "nested operation uses descriptor/pointer/class-like storage and may alias external storage";
    }
  });
  if (!nestedEvidence.empty()) {
    return nestedEvidence;
  }
  return "";
}

ShapeInfo fiap::inferShapeInfo(mlir::Operation &op) {
  const bool preferDestinationOperand =
      op.getName().getStringRef().contains("hlfir.assign") ||
      op.getName().getStringRef().contains("fir.store");
#if FIAP_HAVE_FLANG
  if (!preferDestinationOperand) {
    for (mlir::Type type : op.getResultTypes()) {
      ShapeInfo shape;
      if (fillShapeFromTypedType(type, shape)) {
        return shape;
      }
    }
  }
  if (preferDestinationOperand) {
    for (mlir::Value operand : llvm::reverse(op.getOperands())) {
      ShapeInfo shape;
      if (fillShapeFromTypedType(operand.getType(), shape)) {
        return shape;
      }
    }
  } else {
    for (mlir::Value operand : op.getOperands()) {
      ShapeInfo shape;
      if (fillShapeFromTypedType(operand.getType(), shape)) {
        return shape;
      }
    }
  }
  if (preferDestinationOperand) {
    for (mlir::Type type : op.getResultTypes()) {
      ShapeInfo shape;
      if (fillShapeFromTypedType(type, shape)) {
        return shape;
      }
    }
  }
#endif
  return inferShapeFromTypesGeneric(op, preferDestinationOperand);
}

std::string fiap::summarizeOperation(mlir::Operation &op,
                                     const OperationSemantics &semantics) {
  switch (semantics.construct) {
  case ImplicitConstructKind::ArrayExpressionTemporary:
    return semantics.typedFlangMatch
               ? "typed HLFIR array expression may materialize a temporary"
               : "array expression may materialize a temporary";
  case ImplicitConstructKind::ElementalTemporary:
    return semantics.typedFlangMatch
               ? "typed HLFIR elemental evaluation may materialize a temporary"
               : "elemental evaluation may materialize a temporary";
  case ImplicitConstructKind::FunctionResultTemporary:
    return semantics.typedFlangMatch
               ? "typed FIR/HLFIR function result may materialize a temporary"
               : "array-valued function result may materialize a temporary";
  case ImplicitConstructKind::ReallocOnAssignment:
    return semantics.typedFlangMatch
               ? "typed HLFIR assignment may trigger automatic reallocation"
               : "allocatable assignment may trigger automatic reallocation";
  case ImplicitConstructKind::AssociateTemporary:
    return semantics.typedFlangMatch
               ? "typed HLFIR associate may extend temporary lifetime"
               : "associate region may extend temporary lifetime";
  case ImplicitConstructKind::Unknown:
    break;
  }

  switch (semantics.kind) {
  case APGNodeKind::AllocMem:
    return semantics.typedFlangMatch
               ? "typed FIR heap allocation site"
               : "compiler-generated heap allocation";
  case APGNodeKind::Assign:
    return "assignment consuming a temporary";
  case APGNodeKind::Destroy:
    return "temporary cleanup";
  case APGNodeKind::Call:
    return "call participating in temporary materialization";
  case APGNodeKind::Expression:
  case APGNodeKind::Elemental:
  case APGNodeKind::Associate:
  case APGNodeKind::Unknown:
    break;
  }
  return op.getName().getStringRef().str();
}
