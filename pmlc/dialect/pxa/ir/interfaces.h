// Copyright 2020, Intel Corporation

#pragma once

#include "mlir/Dialect/Affine/IR/AffineValueMap.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "pmlc/dialect/pxa/ir/stride_range.h"

namespace mlir {

// TODO: Lorenzo check fw decl for Memref.

struct OpOperandVector : public SmallVector<OpOperand *> {
  operator SmallVector<Value>();
};

} // namespace mlir

namespace pmlc::dialect::pxa {

struct PxaMemAccessOperand {
  mlir::OpOperand *opOperand;

  explicit PxaMemAccessOperand(mlir::OpOperand *opOperand)
      : opOperand(opOperand) {}

  mlir::Operation *getOperation() const { return opOperand->getOwner(); }

  mlir::Value getMemRef() const { return opOperand->get(); }

  mlir::MemRefType getMemRefType() const {
    return getMemRef().getType().cast<mlir::MemRefType>();
  }

  bool isRead() const;
  bool isWrite() const;

  mlir::SmallVector<StrideRange> getInternalRanges() const;

  mlir::AffineValueMap getAffineValueMap() const;
};

} // namespace pmlc::dialect::pxa

#include "pmlc/dialect/pxa/ir/interfaces.h.inc"
