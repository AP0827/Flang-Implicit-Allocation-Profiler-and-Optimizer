program array_add
  implicit none
  integer, parameter :: n = 100000
  real, allocatable :: a(:), b(:), c(:)
  integer :: i

  allocate(a(n), b(n), c(n))
  do i = 1, n
    b(i) = real(i)
    c(i) = real(2 * i)
  end do

  do i = 1, 100
    a = b + c
  end do
end program array_add
