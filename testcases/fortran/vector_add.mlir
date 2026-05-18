module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "w", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, fir.defaultkind = "a1c4d8i4l4r4", fir.kindmap = "", fir.relocation_model = 1 : i32, llvm.data_layout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.ident = "flang version 23.0.0 (https://github.com/llvm/llvm-project.git 3c3e7e0784befd7b80a7fe265a3e4eb7e7e12d2f)", llvm.target_triple = "x86_64-pc-windows-msvc"} {
  func.func @_QQmain() attributes {fir.bindc_name = "VECTOR_ADD"} {
    %0 = fir.dummy_scope : !fir.dscope
    %1 = fir.address_of(@_QFEa) : !fir.ref<!fir.array<1024xf32>>
    %c1024 = arith.constant 1024 : index
    %2 = fir.shape %c1024 : (index) -> !fir.shape<1>
    %3:2 = hlfir.declare %1(%2) {uniq_name = "_QFEa"} : (!fir.ref<!fir.array<1024xf32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>)
    %4 = fir.address_of(@_QFEb) : !fir.ref<!fir.array<1024xf32>>
    %c1024_0 = arith.constant 1024 : index
    %5 = fir.shape %c1024_0 : (index) -> !fir.shape<1>
    %6:2 = hlfir.declare %4(%5) {uniq_name = "_QFEb"} : (!fir.ref<!fir.array<1024xf32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>)
    %7 = fir.address_of(@_QFEc) : !fir.ref<!fir.array<1024xf32>>
    %c1024_1 = arith.constant 1024 : index
    %8 = fir.shape %c1024_1 : (index) -> !fir.shape<1>
    %9:2 = hlfir.declare %7(%8) {uniq_name = "_QFEc"} : (!fir.ref<!fir.array<1024xf32>>, !fir.shape<1>) -> (!fir.ref<!fir.array<1024xf32>>, !fir.ref<!fir.array<1024xf32>>)
    %10 = fir.address_of(@_QFECn) : !fir.ref<i32>
    %11:2 = hlfir.declare %10 {fortran_attrs = #fir.var_attrs<parameter>, uniq_name = "_QFECn"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
    %cst = arith.constant 1.000000e+00 : f32
    hlfir.assign %cst to %6#0 : f32, !fir.ref<!fir.array<1024xf32>>
    %cst_2 = arith.constant 2.000000e+00 : f32
    hlfir.assign %cst_2 to %9#0 : f32, !fir.ref<!fir.array<1024xf32>>
    %12 = hlfir.elemental %5 unordered : (!fir.shape<1>) -> !hlfir.expr<1024xf32> {
    ^bb0(%arg0: index):
      %20 = hlfir.designate %6#0 (%arg0)  : (!fir.ref<!fir.array<1024xf32>>, index) -> !fir.ref<f32>
      %21 = hlfir.designate %9#0 (%arg0)  : (!fir.ref<!fir.array<1024xf32>>, index) -> !fir.ref<f32>
      %22 = fir.load %20 : !fir.ref<f32>
      %23 = fir.load %21 : !fir.ref<f32>
      %24 = arith.addf %22, %23 fastmath<contract> : f32
      hlfir.yield_element %24 : f32
    }
    hlfir.assign %12 to %3#0 : !hlfir.expr<1024xf32>, !fir.ref<!fir.array<1024xf32>>
    hlfir.destroy %12 : !hlfir.expr<1024xf32>
    %c6_i32 = arith.constant 6 : i32
    %13 = fir.address_of(@_QQclXd4fc26c2e946d7703470d9b3535d57e8) : !fir.ref<!fir.char<1,85>>
    %14 = fir.convert %13 : (!fir.ref<!fir.char<1,85>>) -> !fir.ref<i8>
    %c10_i32 = arith.constant 10 : i32
    %15 = fir.call @_FortranAioBeginExternalListOutput(%c6_i32, %14, %c10_i32) fastmath<contract> : (i32, !fir.ref<i8>, i32) -> !fir.ref<i8>
    %c1 = arith.constant 1 : index
    %16 = hlfir.designate %3#0 (%c1)  : (!fir.ref<!fir.array<1024xf32>>, index) -> !fir.ref<f32>
    %17 = fir.load %16 : !fir.ref<f32>
    %18 = fir.call @_FortranAioOutputReal32(%15, %17) fastmath<contract> : (!fir.ref<i8>, f32) -> i1
    %19 = fir.call @_FortranAioEndIoStatement(%15) fastmath<contract> : (!fir.ref<i8>) -> i32
    return
  }
  fir.global internal @_QFEa : !fir.array<1024xf32> {
    %0 = fir.zero_bits !fir.array<1024xf32>
    fir.has_value %0 : !fir.array<1024xf32>
  }
  fir.global internal @_QFEb : !fir.array<1024xf32> {
    %0 = fir.zero_bits !fir.array<1024xf32>
    fir.has_value %0 : !fir.array<1024xf32>
  }
  fir.global internal @_QFEc : !fir.array<1024xf32> {
    %0 = fir.zero_bits !fir.array<1024xf32>
    fir.has_value %0 : !fir.array<1024xf32>
  }
  fir.global internal @_QFECn constant : i32 {
    %c1024_i32 = arith.constant 1024 : i32
    fir.has_value %c1024_i32 : i32
  }
  func.func private @_FortranAioBeginExternalListOutput(i32, !fir.ref<i8>, i32) -> !fir.ref<i8> attributes {fir.io, fir.runtime}
  fir.global linkonce @_QQclXd4fc26c2e946d7703470d9b3535d57e8 constant : !fir.char<1,85> {
    %0 = fir.string_lit "D:\\Flang-Implicit-Allocation-Profiler-and-Optimizer\\testcases\\fortran\\vector_add.f90\00"(85) : !fir.char<1,85>
    fir.has_value %0 : !fir.char<1,85>
  }
  func.func private @_FortranAioOutputReal32(!fir.ref<i8>, f32) -> i1 attributes {fir.io, fir.runtime}
  func.func private @_FortranAioEndIoStatement(!fir.ref<i8>) -> i32 attributes {fir.io, fir.runtime}
  func.func private @_FortranAProgramStart(i32, !llvm.ptr, !llvm.ptr, !llvm.ptr)
  func.func private @_FortranAProgramEndStatement()
  func.func @main(%arg0: i32, %arg1: !llvm.ptr, %arg2: !llvm.ptr) -> i32 {
    %0 = fir.zero_bits !fir.ref<tuple<i32, !fir.ref<!fir.array<0xtuple<!fir.ref<i8>, !fir.ref<i8>>>>>>
    fir.call @_FortranAProgramStart(%arg0, %arg1, %arg2, %0) fastmath<contract> : (i32, !llvm.ptr, !llvm.ptr, !fir.ref<tuple<i32, !fir.ref<!fir.array<0xtuple<!fir.ref<i8>, !fir.ref<i8>>>>>>) -> ()
    fir.call @_QQmain() fastmath<contract> : () -> ()
    %c0_i32 = arith.constant 0 : i32
    fir.call @_FortranAProgramEndStatement() fastmath<contract> : () -> ()
    return %c0_i32 : i32
  }
}
