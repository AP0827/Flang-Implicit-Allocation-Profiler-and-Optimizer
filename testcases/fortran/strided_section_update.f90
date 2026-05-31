program strided_section_update
  implicit none
  integer, parameter :: n = 1024
  real :: a(n), b(n), c(n)
  real :: total

  a = 0.0
  b = 1.0
  c = 2.0

  a(1:n:2) = b(1:n:2) + c(1:n:2)

  total = sum(a)
  print *, total
end program strided_section_update
