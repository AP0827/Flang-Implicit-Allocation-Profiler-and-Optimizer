program escaping_temp
  implicit none
  integer, parameter :: n = 512
  integer, parameter :: repeats = 200000
  integer :: iter
  real :: b(n), c(n)
  real :: total

  b = 1.0
  c = 2.0
  total = 0.0
  do iter = 1, repeats
    b(1) = b(1) + 1.0e-6
    call consume(b + c)
  end do
  print *, total

contains

  subroutine consume(x)
    real, intent(in) :: x(:)
    total = total + x(1)
  end subroutine consume

end program escaping_temp
