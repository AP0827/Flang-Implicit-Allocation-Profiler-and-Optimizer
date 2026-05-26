// Allocatable assignment with runtime shape: expected possibly-unnecessary.
module {
  %lhs = "fiap.arg"() : () -> !fir.box<!fir.array<?xf32>> loc("realloc_assignment.f90":8:3)
  %rhs = "fiap.arg"() : () -> !hlfir.expr<?xf32> loc("realloc_assignment.f90":8:7)
  "fiap.hlfir.assign"(%rhs, %lhs) {realloc} : (!hlfir.expr<?xf32>, !fir.box<!fir.array<?xf32>>) -> () loc("realloc_assignment.f90":8:3)
}
