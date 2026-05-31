program strided_section_update
  implicit none
  integer, parameter :: n = 1024
  integer :: i
  real :: a(n), b(n), c(n)
  real :: total

  a = 0.0
  b = 1.0
  c = 2.0

  do concurrent (i = 1:n:2)
    a(i) = b(i) + c(i)
  end do

  total = sum(a)
  print *, total
end program strided_section_update
