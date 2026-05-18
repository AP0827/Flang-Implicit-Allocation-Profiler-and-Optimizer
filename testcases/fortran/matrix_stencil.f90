program matrix_stencil
  implicit none
  integer, parameter :: n = 128
  real :: a(n, n), b(n, n), c(n, n)

  b = 1.0
  c = 2.0
  a = b + c * 0.5

  print *, a(1, 1)
end program matrix_stencil
