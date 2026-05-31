program rank3_tensor_update
  implicit none
  integer, parameter :: nx = 12, ny = 10, nz = 8
  integer :: i, j, k
  real :: a(nx, ny, nz), b(nx, ny, nz), c(nx, ny, nz)
  real :: total

  b = 1.0
  c = 2.0

  do concurrent (k = lbound(a, 3):ubound(a, 3), j = lbound(a, 2):ubound(a, 2), i = lbound(a, 1):ubound(a, 1))
    a(i, j, k) = b(i, j, k) + 0.25 * c(i, j, k)
  end do

  total = sum(a)
  print *, total
end program rank3_tensor_update
