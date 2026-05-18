module {
  func.func @resize_assign(%lhs: !fir.box<!fir.array<?xf32>>,
                           %rhs: !hlfir.expr<?xf32>) loc("resize_assign.f90":9:3) {
    "hlfir.assign"(%rhs, %lhs) {realloc = true} : (!hlfir.expr<?xf32>, !fir.box<!fir.array<?xf32>>) -> () loc("resize_assign.f90":9:3)
    return loc("resize_assign.f90":10:1)
  }
}
