program vector_add
  implicit none
  integer, parameter :: n = 1024
  integer, parameter :: repeats = 200000
  integer :: iter
  real :: a(n), b(n), c(n)
  real :: total

  b = 1.0
  c = 2.0
  total = 0.0

  do iter = 1, repeats
    b(1) = b(1) + 1.0e-6
    a = b + c
    total = total + a(1)
  end do
  print *, total + sum(a)
end program vector_add
