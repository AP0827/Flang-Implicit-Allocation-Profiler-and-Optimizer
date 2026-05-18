// RUN: fiap-opt %s --format=json | FileCheck %s
// CHECK: "construct":"function-result-temporary"
// CHECK: "possibly-unnecessary"

module {
  func.func @function_result(%arg0: !fir.ref<!fir.array<256xf32>>) loc("function-result.f90":20:3) {
    %0 = "fir.call"() : () -> !fir.box<!fir.array<256xf32>> loc("function-result.f90":20:10)
    "hlfir.assign"(%0, %arg0) : (!fir.box<!fir.array<256xf32>>, !fir.ref<!fir.array<256xf32>>) -> () loc("function-result.f90":20:3)
    return loc("function-result.f90":21:1)
  }
}
