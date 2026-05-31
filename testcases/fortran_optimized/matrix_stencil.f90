program matrix_stencil
  implicit none
  integer, parameter :: n = 128
  integer, parameter :: repeats = 20000
  integer :: i, j, iter
  real :: a(n, n), b(n, n), c(n, n)
  real :: total

  b = 1.0
  c = 2.0
  total = 0.0
  do iter = 1, repeats
    b(1, 1) = b(1, 1) + 1.0e-6
    do j = lbound(a, 2), ubound(a, 2)
      do i = lbound(a, 1), ubound(a, 1)
        a(i, j) = b(i, j) + c(i, j) * 0.5
      end do
    end do
    total = total + a(1, 1)
  end do

  print *, total + sum(a)
end program matrix_stencil
