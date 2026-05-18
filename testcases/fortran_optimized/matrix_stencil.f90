program matrix_stencil
  implicit none
  integer, parameter :: n = 128
  integer :: i, j
  real :: a(n, n), b(n, n), c(n, n)

  b = 1.0
  c = 2.0
  do concurrent (j = lbound(a, 2):ubound(a, 2), i = lbound(a, 1):ubound(a, 1))
    a(i, j) = b(i, j) + c(i, j) * 0.5
  end do

  print *, a(1, 1)
end program matrix_stencil
