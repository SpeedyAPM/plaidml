// Copyright 2019, Intel Corporation

#include "pmlc/dialect/tile/ir/ops.h"

#include <vector>

#include "mlir/IR/BlockAndValueMapping.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/DebugStringHelper.h"

#include "pmlc/dialect/tile/ir/util.h"
#include "pmlc/util/logging.h"
#include "pmlc/util/util.h"

using namespace mlir; // NOLINT

namespace pmlc::dialect::tile {

using llvm::SmallVector;

LogicalResult ArgSortOp::materializeOperands(OpBuilder &builder) {
  return tile::materializeOperands(builder, getOperation());
}

LogicalResult ContractionOp::materializeOperands(OpBuilder &builder) {
  Operation *op = getOperation();
  if (combo() == CombinationKind::cond) {
    auto operands = op->getOpOperands();
    return success(
        succeeded(tile::materializeOperands(
            builder, op,
            llvm::ArrayRef<OpOperand *>{&operands[0], &operands[3]})) &&
        succeeded(tile::materializeOperands(
            builder, op,
            llvm::ArrayRef<OpOperand *>{&operands[1], &operands[2]})));
  }
  return tile::materializeOperands(builder, getOperation());
}

struct SimplifyContractionOp : public OpRewritePattern<ContractionOp> {
  using OpRewritePattern<ContractionOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ContractionOp op,
                                PatternRewriter &rewriter) const override {
    op.setSink(simplifyAffineMap(op.sink()));
    SmallVector<AffineMap> srcs;
    for (AffineMap src : op.srcs().getAsValueRange<AffineMapAttr>()) {
      srcs.push_back(simplifyAffineMap(src));
    }
    op.setSources(srcs);
    return success();
  }
};

void ContractionOp::getCanonicalizationPatterns(RewritePatternSet &patterns,
                                                MLIRContext *context) {
  patterns.insert<SimplifyContractionOp>(patterns.getContext());
}

LogicalResult GatherOp::materializeOperands(OpBuilder &builder) {
  Operation *op = getOperation();
  return tile::materializeOperands(builder, op,
                                   op->getOpOperands().take_front());
}

LogicalResult ReshapeOp::materializeOperands(OpBuilder &builder) {
  return tile::materializeOperands(builder, getOperation());
}

LogicalResult ScatterOp::materializeOperands(OpBuilder &builder) {
  Operation *op = getOperation();
  return tile::materializeOperands(builder, op,
                                   op->getOpOperands().take_front());
}

LogicalResult ShapeOp::materializeOperands(OpBuilder &builder) {
  return tile::materializeOperands(builder, getOperation());
}

LogicalResult PragmaOp::materializeOperands(OpBuilder &builder) {
  return tile::materializeOperands(builder, getOperation());
}

// ---- ReshapeOp ----

OpFoldResult ReshapeOp::fold(ArrayRef<Attribute> operands) {
  // reshape(x, x.shape) -> x
  if (tensor().getType() == getType()) {
    return tensor();
  }
  return {};
}

// ---- ContractionOp ----

unsigned ContractionOp::getNumTensors(CombinationKind combo) {
  switch (combo) {
  case CombinationKind::none:
    return 1;
  case CombinationKind::add:
  case CombinationKind::eq:
  case CombinationKind::mul:
    return 2;
  case CombinationKind::cond:
    return 3;
  default:
    throw std::runtime_error("Invalid combination op");
  }
}

void ContractionOp::build(OpBuilder &builder, OperationState &result,
                          Type resultType, Value init, ValueRange tensors,
                          AggregationKind agg, CombinationKind combo,
                          AffineMap sink, ArrayRef<AffineMap> srcs,
                          IntegerSet cons, StringRef name) {
  result.addOperands(init);
  result.addOperands(tensors);
  result.addTypes(resultType);
  result.addAttribute("agg",
                      builder.getI64IntegerAttr(static_cast<int64_t>(agg)));
  result.addAttribute("combo",
                      builder.getI64IntegerAttr(static_cast<int64_t>(combo)));
  result.addAttribute(getSinkAttrName(), AffineMapAttr::get(sink));
  result.addAttribute(getSourcesAttrName(),
                      builder.getAffineMapArrayAttr(srcs));
  if (!cons.isEmptyIntegerSet()) {
    result.addAttribute(getConstraintsAttrName(), IntegerSetAttr::get(cons));
  }
  if (name.size()) {
    result.addAttribute("name", builder.getStringAttr(name));
  }
}

AffineMap ContractionOp::getSourceMap(unsigned i) {
  return srcs().getValue()[i].cast<AffineMapAttr>().getValue();
}

void ContractionOp::setLowerBounds(ArrayRef<int64_t> bounds) {
  SmallVector<AffineExpr, 6> exprs;
  for (auto dim : bounds) {
    exprs.push_back(getAffineConstantExpr(dim, getContext()));
  }
  auto map =
      AffineMap::get(/*dimCount=*/0, /*symbolCount=*/0, exprs, getContext());
  (*this)->setAttr(getLowerBoundsAttrName(), AffineMapAttr::get(map));
}

void ContractionOp::setUpperBounds(ArrayRef<int64_t> bounds) {
  SmallVector<AffineExpr, 6> exprs;
  for (auto dim : bounds) {
    exprs.push_back(getAffineConstantExpr(dim, getContext()));
  }
  auto map =
      AffineMap::get(/*dimCount=*/0, /*symbolCount=*/0, exprs, getContext());
  (*this)->setAttr(getUpperBoundsAttrName(), AffineMapAttr::get(map));
}

void ContractionOp::setSink(AffineMap sink) {
  (*this)->setAttr(getSinkAttrName(), AffineMapAttr::get(sink));
}

void ContractionOp::setSources(ArrayRef<AffineMap> srcs) {
  SmallVector<Attribute, 4> attrs;
  for (auto src : srcs) {
    attrs.push_back(AffineMapAttr::get(src));
  }
  (*this)->setAttr(getSourcesAttrName(), ArrayAttr::get(getContext(), attrs));
}

void ContractionOp::setConstraints(IntegerSet cons) {
  if (cons.isEmptyIntegerSet()) {
    (*this)->removeAttr(getConstraintsAttrName());
  } else {
    (*this)->setAttr(getConstraintsAttrName(), IntegerSetAttr::get(cons));
  }
}

unsigned ContractionOp::getNumTensors() { return getNumTensors(combo()); }

unsigned ContractionOp::getNumSymbols() {
  return getNumOperands() - 1 - getNumTensors();
}

Value ContractionOp::getTensor(unsigned i) {
  return *std::next(operands().begin(), i);
}

Value ContractionOp::getSymbol(unsigned i) {
  return *std::next(operands().begin(), getNumTensors() + i);
}

void ContractionOp::print(OpAsmPrinter &printer) {
  SmallVector<StringRef, 3> elidedAttrs = {"agg", "combo", "name"};
  printer << ' ' << util::stringifyAggregationKind(agg());
  printer << ", ";
  printer << util::stringifyCombinationKind(combo());
  printer << ", ";
  printer.printOperand(init());
  auto numTensors = getNumTensors();
  for (unsigned i = 0; i < numTensors; i++) {
    printer << ", ";
    printer.printOperand(getTensor(i));
  }
  auto numSymbols = getNumSymbols();
  if (numSymbols) {
    printer << " [";
    for (unsigned i = 0; i < numSymbols; i++) {
      if (i) {
        printer << ", ";
      }
      printer.printOperand(getSymbol(i));
    }
    printer << ']';
  }
  printer.printOptionalAttrDict((*this)->getAttrs(), elidedAttrs);
  printer << " : ";
  printer.printType(init().getType());
  printer << ", ";
  for (unsigned i = 0; i < numTensors; i++) {
    if (i) {
      printer << ", ";
    }
    printer.printType(getTensor(i).getType());
  }
  printer << " -> ";
  printer.printType(result().getType());
}

ParseResult ContractionOp::parse(OpAsmParser &parser, OperationState &result) {
  StringRef strAgg;
  StringRef strCombo;
  OpAsmParser::UnresolvedOperand init;
  SmallVector<OpAsmParser::UnresolvedOperand, 3> tensors;
  SmallVector<OpAsmParser::UnresolvedOperand, 8> symbols;
  SmallVector<Type, 4> types;
  Type resultType;
  if (parser.parseKeyword(&strAgg) || parser.parseComma() ||
      parser.parseKeyword(&strCombo) || parser.parseComma() ||
      parser.parseOperand(init) || parser.parseComma()) {
    return failure();
  }

  auto agg = util::symbolizeAggregationKind(strAgg);
  if (!agg) {
    return failure();
  }
  result.addAttribute("agg", parser.getBuilder().getI64IntegerAttr(
                                 static_cast<int64_t>(agg.getValue())));

  auto combo = util::symbolizeCombinationKind(strCombo);
  if (!combo) {
    return failure();
  }
  result.addAttribute("combo", parser.getBuilder().getI64IntegerAttr(
                                   static_cast<int64_t>(combo.getValue())));

  auto numTensors = ContractionOp::getNumTensors(combo.getValue());
  if (parser.parseOperandList(tensors, numTensors) ||
      parser.parseOperandList(symbols,
                               OpAsmParser::Delimiter::OptionalSquare) ||
      parser.parseOptionalAttrDict(result.attributes) ||
      parser.parseColonTypeList(types) || parser.parseArrow() ||
      parser.parseType(resultType)) {
    return failure();
  }

  // TODO: parse a FunctionType here

  auto loc = parser.getCurrentLocation();
  auto indexType = parser.getBuilder().getIndexType();
  auto tensorTypes = llvm::makeArrayRef(types).drop_front();
  if (parser.resolveOperand(init, types.front(), result.operands) ||
      parser.resolveOperands(tensors, tensorTypes, loc, result.operands) ||
      parser.resolveOperands(symbols, indexType, result.operands)) {
    return failure();
  }

  result.addTypes(resultType);
  return success();
}

bool isAnyScalar(Type type) {
  return type.isIndex() || type.isa<FloatType>() || type.isInteger(1) ||
         type.isSignedInteger() || type.isUnsignedInteger();
}

bool isEltwiseAny(Type type) {
  if (auto rankedTensorType = type.dyn_cast<RankedTensorType>()) {
    auto elementType = rankedTensorType.getElementType();
    return isAnyScalar(elementType);
  }
  return isAnyScalar(type);
}

LogicalResult ContractionOp::verify() {
  auto numTensors = getNumTensors();
  auto numSymbols = getNumSymbols();
  SmallVector<Value, 8> variadic(operands());
  if (variadic.size() < numTensors) {
    return emitOpError("combo '")
           << util::stringifyCombinationKind(combo()) << "' requires "
           << numTensors << " tensor operands";
  }
  auto s = shape();
  auto resultType = result().getType().cast<RankedTensorType>();
  if (!resultType.hasStaticShape() && !s.hasValue()) {
    return emitOpError(
        "attribute 'shape' is required when result type is dynamic");
  }
  unsigned expectedSymbols = sink().getNumSymbols();
  if (s.hasValue()) {
    expectedSymbols += s->getNumSymbols();
  }
  for (auto src : srcs()) {
    auto map = src.cast<AffineMapAttr>();
    expectedSymbols += map.getValue().getNumSymbols();
  }
  if (cons().hasValue()) {
    expectedSymbols += cons().getValue().getNumSymbols();
  }
  if (expectedSymbols != numSymbols) {
    return emitOpError("has incorrect number of symbols: expected ")
           << expectedSymbols << " but found " << numSymbols;
  }
  for (unsigned i = 0; i < numTensors; i++) {
    auto type = getTensor(i).getType();
    if (!isEltwiseAny(type)) {
      return emitOpError("tensor #")
             << i << " must be eltwise-any, but got " << type;
    }
  }
  for (unsigned i = 0; i < numSymbols; i++) {
    auto type = getSymbol(i).getType();
    if (!type.isa<IndexType>()) {
      return emitOpError("symbol #")
             << i << " must be index, but got " << type;
    }
  }
  return success();
}

LogicalResult ReshapeOp::verify() {
  auto inType = tensor().getType().cast<RankedTensorType>();
  auto outType = result().getType().cast<RankedTensorType>();
  if (inType.getElementType() != outType.getElementType()) {
    return emitOpError("element type mismatch");
  }
  if (inType.getNumElements() != outType.getNumElements()) {
    return emitOpError("element count mismatch");
  }
  return success();
}

void GatherOp::build(OpBuilder &builder, OperationState &result,
                     Type resultType, ValueRange operands, IntegerAttr axis,
                     IntegerAttr interpolationMode, IntegerAttr nearestMode,
                     FloatAttr cubeCoeff, IntegerAttr mode,
                     IntegerAttr batchDims, IntegerAttr OutOfBoundsMode) {
  assert(operands.size() == 2u && "mismatched number of parameters");
  result.addOperands(operands);
  result.addAttribute("axis", axis);
  result.addAttribute("interpolationMode", interpolationMode);
  result.addAttribute("nearestMode", nearestMode);
  result.addAttribute("cubeCoeff", cubeCoeff);
  result.addAttribute("mode", mode);
  result.addAttribute("batchDims", batchDims);
  result.addAttribute("OutOfBoundsMode", OutOfBoundsMode);
  result.addTypes(resultType);
}

void ScatterOp::build(OpBuilder &builder, OperationState &result,
                      Type resultType, ValueRange operands, IntegerAttr axis,
                      IntegerAttr mode) {
  assert(operands.size() == 3u && "mismatched number of parameters");
  result.addOperands(operands);
  result.addAttribute("axis", axis);
  result.addAttribute("mode", mode);
  result.addTypes(resultType);
}

} // namespace pmlc::dialect::tile

#include "pmlc/dialect/tile/ir/enums.cc.inc"

#define GET_OP_CLASSES
#include "pmlc/dialect/tile/ir/ops.cc.inc"

#include "pmlc/dialect/tile/ir/interfaces.cc.inc"
