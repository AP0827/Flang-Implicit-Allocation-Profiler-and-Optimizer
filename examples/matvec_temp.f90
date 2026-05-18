program matvec_temp
  implicit none
  integer, parameter :: n = 1024
  real :: a(n), b(n), c(n)
  integer :: i

  do i = 1, n
    b(i) = real(i)
    c(i) = 2.0 * real(i)
  end do

  do i = 1, 5000
    a = b + c
  end do
end program matvec_temp
