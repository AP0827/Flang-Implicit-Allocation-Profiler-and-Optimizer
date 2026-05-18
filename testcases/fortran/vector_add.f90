program vector_add
  implicit none
  integer, parameter :: n = 1024
  real :: a(n), b(n), c(n)

  b = 1.0
  c = 2.0

  a = b + c
  print *, a(1)
end program vector_add
