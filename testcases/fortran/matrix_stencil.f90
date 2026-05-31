program matrix_stencil
  implicit none
  integer, parameter :: n = 128
  integer, parameter :: repeats = 20000
  integer :: iter
  real :: a(n, n), b(n, n), c(n, n)
  real :: total

  b = 1.0
  c = 2.0
  total = 0.0
  do iter = 1, repeats
    b(1, 1) = b(1, 1) + 1.0e-6
    a = b + c * 0.5
    total = total + a(1, 1)
  end do

  print *, total + sum(a)
end program matrix_stencil
