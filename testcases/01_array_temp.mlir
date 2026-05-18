// Array expression temporary: expected provably-eliminable.
module {
  %b = "fiap.arg"() : () -> !fir.ref<!fir.array<1024xf32>> loc("vector_add.f90":9:7)
  %c = "fiap.arg"() : () -> !fir.ref<!fir.array<1024xf32>> loc("vector_add.f90":9:11)
  %a = "fiap.arg"() : () -> !fir.ref<!fir.array<1024xf32>> loc("vector_add.f90":9:3)
  %0 = "hlfir.as_expr"(%b, %c) : (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>) -> !hlfir.expr<1024xf32> loc("vector_add.f90":9:7)
  %1 = "fir.allocmem"() : () -> !fir.heap<!fir.array<1024xf32>> loc("vector_add.f90":9:7)
  "hlfir.assign"(%0, %a) : (!hlfir.expr<1024xf32>, !fir.ref<!fir.array<1024xf32>>) -> () loc("vector_add.f90":9:3)
  "hlfir.destroy"(%0) : (!hlfir.expr<1024xf32>) -> () loc("vector_add.f90":9:3)
}
