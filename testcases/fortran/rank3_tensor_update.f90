program rank3_tensor_update
  implicit none
  integer, parameter :: nx = 12, ny = 10, nz = 8
  real :: a(nx, ny, nz), b(nx, ny, nz), c(nx, ny, nz)
  real :: total

  b = 1.0
  c = 2.0

  a = b + 0.25 * c

  total = sum(a)
  print *, total
end program rank3_tensor_update
