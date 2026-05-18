// RUN: fiap-opt %s --format=json | FileCheck %s
// CHECK: "construct":"realloc-on-assignment"
// CHECK: "possibly-unnecessary"

module {
  func.func @resize_assign(%arg0: !fir.box<!fir.array<?xf32>>,
                           %arg1: !hlfir.expr<?xf32>) loc("realloc-assignment.f90":12:3) {
    "hlfir.assign"(%arg1, %arg0) {realloc = true} : (!hlfir.expr<?xf32>, !fir.box<!fir.array<?xf32>>) -> () loc("realloc-assignment.f90":12:3)
    return loc("realloc-assignment.f90":13:1)
  }
}
