module {
  func.func @array_temp(%a: !fir.ref<!fir.array<1024xf32>>,
                        %b: !fir.ref<!fir.array<1024xf32>>,
                        %c: !fir.ref<!fir.array<1024xf32>>) loc("matvec_temp.f90":11:5) {
    %0 = "hlfir.as_expr"(%b, %c) : (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>) -> !hlfir.expr<1024xf32> loc("matvec_temp.f90":11:9)
    %1 = "fir.allocmem"() : () -> !fir.heap<!fir.array<1024xf32>> loc("matvec_temp.f90":11:9)
    "hlfir.assign"(%0, %a) : (!hlfir.expr<1024xf32>, !fir.ref<!fir.array<1024xf32>>) -> () loc("matvec_temp.f90":11:5)
    "hlfir.destroy"(%0) : (!hlfir.expr<1024xf32>) -> () loc("matvec_temp.f90":11:5)
    return loc("matvec_temp.f90":12:1)
  }
}
