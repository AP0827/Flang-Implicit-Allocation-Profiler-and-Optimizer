module {
  func.func @function_result(%arg0: !fir.ref<!fir.array<256xf32>>) loc("function_result.f90":15:3) {
    %0 = "fir.call"() : () -> !fir.box<!fir.array<256xf32>> loc("function_result.f90":15:10)
    "hlfir.assign"(%0, %arg0) : (!fir.box<!fir.array<256xf32>>, !fir.ref<!fir.array<256xf32>>) -> () loc("function_result.f90":15:3)
    return loc("function_result.f90":16:1)
  }
}
