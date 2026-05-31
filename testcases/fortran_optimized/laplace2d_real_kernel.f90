program laplace2d_real_kernel
  implicit none
  integer, parameter :: n = 96
  integer, parameter :: repeats = 25000
  integer :: i, j, iter
  real :: north(n, n), south(n, n), east(n, n), west(n, n)
  real :: next(n, n)
  real :: total

  north = 1.0
  south = 2.0
  east = 3.0
  west = 4.0
  total = 0.0

  do iter = 1, repeats
    north(1, 1) = north(1, 1) + 1.0e-6
    do concurrent (j = lbound(next, 2):ubound(next, 2), i = lbound(next, 1):ubound(next, 1))
      next(i, j) = 0.25 * (north(i, j) + south(i, j) + east(i, j) + west(i, j))
    end do
    total = total + next(1, 1)
  end do

  print *, total + sum(next)
end program laplace2d_real_kernel
