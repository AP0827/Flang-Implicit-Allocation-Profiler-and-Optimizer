program vector_add
  implicit none
  integer, parameter :: n = 1024
  integer :: i
  real :: a(n), b(n), c(n)

  b = 1.0
  c = 2.0

  do concurrent (i = lbound(a, 1):ubound(a, 1))
    a(i) = b(i) + c(i)
  end do
  print *, a(1)
end program vector_add
