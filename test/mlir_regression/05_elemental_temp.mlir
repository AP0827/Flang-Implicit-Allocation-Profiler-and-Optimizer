// Elemental expression temporary: expected provably-eliminable.
module {
  %a = "fiap.arg"() : () -> !fir.ref<!fir.array<2048xf32>> loc("elemental_temp.f90":9:3)
  %b = "fiap.arg"() : () -> !fir.ref<!fir.array<2048xf32>> loc("elemental_temp.f90":9:7)
  %0 = "fiap.hlfir.elemental"(%b) : (!fir.ref<!fir.array<2048xf32>>) -> !hlfir.expr<2048xf32> loc("elemental_temp.f90":9:7)
  %1 = "fiap.fir.allocmem"() : () -> !fir.heap<!fir.array<2048xf32>> loc("elemental_temp.f90":9:7)
  "fiap.hlfir.assign"(%0, %a) : (!hlfir.expr<2048xf32>, !fir.ref<!fir.array<2048xf32>>) -> () loc("elemental_temp.f90":9:3)
  "fiap.hlfir.destroy"(%0) : (!hlfir.expr<2048xf32>) -> () loc("elemental_temp.f90":9:3)
}
