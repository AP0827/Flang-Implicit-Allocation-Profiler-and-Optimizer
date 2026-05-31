program assumed_shape_kernel
  implicit none
  integer, parameter :: n = 2048
  real :: a(n), b(n), c(n)
  real :: total

  b = 1.0
  c = 3.0
  call update(a, b, c)

  total = sum(a)
  print *, total

contains
  subroutine update(a, b, c)
    real, intent(out) :: a(:)
    real, intent(in) :: b(:), c(:)

    a = b + c
  end subroutine update
end program assumed_shape_kernel
