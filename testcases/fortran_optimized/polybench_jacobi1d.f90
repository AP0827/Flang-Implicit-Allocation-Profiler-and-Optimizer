program polybench_jacobi1d
  implicit none
  integer, parameter :: n = 4096, steps = 40
  integer :: i, iter
  real :: a(n), b(n), left(n), right(n)
  real :: total

  a = 1.0
  b = 2.0
  left = 0.5
  right = 1.5

  do iter = 1, steps
    do concurrent (i = lbound(b, 1):ubound(b, 1))
      b(i) = 0.3333333 * (left(i) + a(i) + right(i))
    end do
    do concurrent (i = lbound(a, 1):ubound(a, 1))
      a(i) = b(i) + 0.125 * a(i)
    end do
  end do

  total = sum(a)
  print *, total
end program polybench_jacobi1d
