// Copyright 2020 Intel Corporation

#ifndef __PML_STDX_OPS__
#define __PML_STDX_OPS__

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/LoopLikeInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/ViewLikeInterface.td"

def StdX_Dialect : Dialect {
  let name = "stdx";
  let cppNamespace = "pmlc::dialect::stdx";
}

class StdX_Op<string mnemonic, list<Trait> traits = []> :
    Op<StdX_Dialect, mnemonic, traits> {
  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

def ReshapeOp : StdX_Op<"reshape",
  [DeclareOpInterfaceMethods<ViewLikeOpInterface>]> {
  let summary = "memref reshape operation";
  let arguments = (ins AnyMemRef:$tensor);
  let results = (outs AnyMemRef:$result);
  let assemblyFormat = [{
    `(` $tensor `)` attr-dict `:` functional-type($tensor, results)
  }];
}

class StdX_UnaryFuncOp<string mnemonic, list<Trait> traits = [NoSideEffect]> :
    Op<StdX_Dialect, mnemonic, traits> {
  let arguments = (ins AnyType:$value);
  let results = (outs AnyType);
  let assemblyFormat = [{
    `(` $value `)` attr-dict `:` functional-type(operands, results)
  }];
}

def ACosOp : StdX_UnaryFuncOp<"acos"> {
  let summary = "arccosine";
}

def ACosHOp : StdX_UnaryFuncOp<"acosh"> {
  let summary = "hyperbolic arccosine";
}

def ASinOp : StdX_UnaryFuncOp<"asin"> {
  let summary = "arcsine";
}

def ASinHOp : StdX_UnaryFuncOp<"asinh"> {
  let summary = "hyperbolic arcsine";
}

def ATanHOp : StdX_UnaryFuncOp<"atanh"> {
  let summary = "hyperbolic arctangent";
}

def CosHOp : StdX_UnaryFuncOp<"cosh"> {
  let summary = "hyperbolic cosine";
}

def ReluOp : StdX_UnaryFuncOp<"relu"> {
  let summary = "ReLU operation";
}

def RoundOp : StdX_UnaryFuncOp<"round"> {
  let summary = "round to nearest integer";
}

def SinHOp : StdX_UnaryFuncOp<"sinh"> {
  let summary = "hyperbolic sine";
}

def TanOp : StdX_UnaryFuncOp<"tan"> {
  let summary = "tangent";
}

def AnyStdScalar : AnyTypeOf<[AnyFloat, AnySignlessInteger]> {}
def AnyVectorOrScalar : AnyTypeOf<[AnyStdScalar, AnyVector]> {}

def SubgroupBroadcastOp : StdX_Op<"subgroup_broadcast", [NoSideEffect, AllTypesMatch<["value", "result"]>]> {
  let summary = "broadcast to all elements in subgroup";
  let arguments = (ins AnyStdScalar:$value, Index:$localid);
  let results = (outs AnyStdScalar:$result);
  let assemblyFormat = [{
    `(` $value `,` $localid `)` attr-dict `:` type($value)
  }];
}

def SubgroupBlockReadINTELOp : StdX_Op<"subgroup_block_read_intel", [NoSideEffect]> {
  let summary = "See intel_subgroups extension.";
  let arguments = (ins Arg<AnyMemRef, "the reference to subgroup block read from",
                           [MemRead]>:$memref,
                           Variadic<Index>:$indices);
  let results = (outs AnyVectorOrScalar:$result);

  let builders = [
    OpBuilder<(ins
      "mlir::Value":$memref,
      CArg<"mlir::ValueRange", "{}">:$indices
    )>
  ];

  let extraClassDeclaration = [{
    mlir::MemRefType getMemRefType() {
      return memref().getType().cast<mlir::MemRefType>();
    }
  }];

  let assemblyFormat = "$memref `[` $indices `]` attr-dict `:` type($memref)`,` type($result)";
}

def SubgroupBlockWriteINTELOp : StdX_Op<"subgroup_block_write_intel", [NoSideEffect]> {
  let summary = "See intel_subgroups extension.";

  let arguments = (ins AnyVectorOrScalar:$value,
                       Arg<AnyMemRef, "the reference to subgroup block write to",
                       [MemWrite]>:$memref,
                       Variadic<Index>:$indices);

  let builders = [
    OpBuilder<(ins "mlir::Value":$valueToStore, "mlir::Value":$memref)>
  ];

  let extraClassDeclaration = [{
      mlir::MemRefType getMemRefType() {
        return memref().getType().cast<mlir::MemRefType>();
      }
  }];

  let assemblyFormat = [{
    $value `,` $memref `[` $indices `]` attr-dict `:` type($value) `,` type($memref)
  }];
}

def PackOp : Op<StdX_Dialect, "pack", [NoSideEffect]> {
  let summary = "Pack multiple values into a tuple";
  let arguments = (ins Variadic<AnyType>:$in);
  let results = (outs AnyTuple:$out);
  let assemblyFormat = [{ `(` $in `)` attr-dict `:` functional-type(operands, results) }];
}

def UnpackOp : Op<StdX_Dialect, "unpack", [NoSideEffect]> {
  let summary = "Unpack multiple values from a tuple";
  let arguments = (ins AnyTuple:$in);
  let results = (outs Variadic<AnyType>:$out);
  let assemblyFormat = [{ `(` $in `)` attr-dict `:` functional-type(operands, results) }];
}

def ClosureOp : StdX_Op<"closure", [
  DeclareOpInterfaceMethods<LoopLikeOpInterface>
]> {
  let arguments = (ins TypeAttr:$type);
  let regions = (region SizedRegion<1>:$body);

  let hasCustomAssemblyFormat = 1;
  let extraClassDeclaration = [{
      mlir::FunctionType getFunctionType() { return type().cast<mlir::FunctionType>(); }
      void setType(mlir::FunctionType type) {
        typeAttr(mlir::TypeAttr::get(type));
      }
      mlir::Region &getBody() { return body(); }
  }];
}

def YieldOp : StdX_Op<"yield", [
  HasParent<"ClosureOp">,
  NoSideEffect,
  Terminator
]> {
  let arguments = (ins Variadic<AnyType>:$operands);
  let assemblyFormat = "attr-dict ($operands^ `:` type($operands))?";
}

#endif // __PML_STDX_OPS__
