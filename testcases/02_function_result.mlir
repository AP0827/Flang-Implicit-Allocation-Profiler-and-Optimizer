// Array-valued function result: expected possibly-unnecessary.
module {
  %out = "fiap.arg"() : () -> !fir.ref<!fir.array<256xf64>> loc("function_result_case.f90":10:3)
  %0 = "fir.call"() : () -> !fir.box<!fir.array<256xf64>> loc("function_result_case.f90":10:9)
  "hlfir.assign"(%0, %out) : (!fir.box<!fir.array<256xf64>>, !fir.ref<!fir.array<256xf64>>) -> () loc("function_result_case.f90":10:3)
}
