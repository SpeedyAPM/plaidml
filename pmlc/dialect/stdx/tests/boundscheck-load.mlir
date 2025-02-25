// RUN: pmlc-opt -stdx-check-bounds %s | FileCheck %s

module {
  func @simpleLoad(%A: memref<20x10xf32>, %i: index, %j: index) -> (f32) {
    %0 = memref.load %A[%i, %j] : memref<20x10xf32>
    return %0: f32
  }
  // CHECK: %{{.*}} = arith.constant 20 : i64
  // CHECK-NEXT call @plaidml_rt_bounds_check(%arg1, %{{.*}}) : (index, i64) -> ()
  // CHECK-NEXT %{{.*}} = arith.constant 10 : i64
  // CHECK-NEXT call @plaidml_rt_bounds_check(%arg2, %{{.*}}) : (index, i64) -> ()
  // CHECK-NEXT %{{.*}} = memref.load %arg0[%arg1, %arg2] : memref<20x10xf32>
}