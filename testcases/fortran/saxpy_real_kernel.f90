program saxpy_real_kernel
  implicit none
  integer, parameter :: n = 4096
  integer, parameter :: repeats = 60000
  integer :: iter
  real :: x(n), y(n)
  real :: alpha, total

  x = 1.0
  y = 2.0
  alpha = 0.25
  total = 0.0

  do iter = 1, repeats
    x(1) = x(1) + 1.0e-6
    y = alpha * x + y
    total = total + y(1)
  end do

  print *, total + sum(y)
end program saxpy_real_kernel
