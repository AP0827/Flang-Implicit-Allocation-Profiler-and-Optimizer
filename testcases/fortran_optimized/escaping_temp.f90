program escaping_temp
  implicit none
  integer, parameter :: n = 512
  real :: b(n), c(n)

  b = 1.0
  c = 2.0
  call consume(b + c)

contains

  subroutine consume(x)
    real, intent(in) :: x(:)
    print *, x(1)
  end subroutine consume

end program escaping_temp
