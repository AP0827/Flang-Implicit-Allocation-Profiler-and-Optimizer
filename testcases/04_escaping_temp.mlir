// Temporary escapes into a call: expected necessary failure case.
module {
  %b = "fiap.arg"() : () -> !fir.ref<!fir.array<512xf32>> loc("escaping_temp.f90":8:14)
  %c = "fiap.arg"() : () -> !fir.ref<!fir.array<512xf32>> loc("escaping_temp.f90":8:18)
  %0 = "hlfir.as_expr"(%b, %c) : (!fir.ref<!fir.array<512xf32>>, !fir.ref<!fir.array<512xf32>>) -> !hlfir.expr<512xf32> loc("escaping_temp.f90":8:14)
  "fir.call"(%0) : (!hlfir.expr<512xf32>) -> () loc("escaping_temp.f90":8:3)
  "hlfir.destroy"(%0) : (!hlfir.expr<512xf32>) -> () loc("escaping_temp.f90":9:3)
}
