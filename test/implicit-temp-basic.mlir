// RUN: fiap-opt %s | FileCheck %s
// CHECK: Summary:
// CHECK: provably-eliminable
// CHECK: array expression may materialize a temporary

module {
  func.func @array_temp(%arg0: !fir.ref<!fir.array<1024xf32>>,
                        %arg1: !fir.ref<!fir.array<1024xf32>>,
                        %arg2: !fir.ref<!fir.array<1024xf32>>) loc("implicit-temp-basic.f90":7:3) {
    %0 = "hlfir.as_expr"(%arg1, %arg2) : (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>) -> !hlfir.expr<1024xf32> loc("implicit-temp-basic.f90":7:7)
    %1 = "fir.allocmem"() : () -> !fir.heap<!fir.array<1024xf32>> loc("implicit-temp-basic.f90":7:7)
    "hlfir.assign"(%0, %arg0) : (!hlfir.expr<1024xf32>, !fir.ref<!fir.array<1024xf32>>) -> () loc("implicit-temp-basic.f90":7:3)
    "hlfir.destroy"(%0) : (!hlfir.expr<1024xf32>) -> () loc("implicit-temp-basic.f90":7:3)
    return loc("implicit-temp-basic.f90":8:1)
  }
}
