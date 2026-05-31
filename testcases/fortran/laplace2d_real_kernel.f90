program laplace2d_real_kernel
  implicit none
  integer, parameter :: n = 96
  integer, parameter :: repeats = 25000
  integer :: iter
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
    next = 0.25 * (north + south + east + west)
    total = total + next(1, 1)
  end do

  print *, total + sum(next)
end program laplace2d_real_kernel
