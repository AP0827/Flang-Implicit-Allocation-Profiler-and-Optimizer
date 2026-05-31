program polybench_jacobi1d
  implicit none
  integer, parameter :: n = 4096, steps = 40
  integer :: iter
  real :: a(n), b(n), left(n), right(n)
  real :: total

  a = 1.0
  b = 2.0
  left = 0.5
  right = 1.5

  do iter = 1, steps
    b = 0.3333333 * (left + a + right)
    a = b + 0.125 * a
  end do

  total = sum(a)
  print *, total
end program polybench_jacobi1d
