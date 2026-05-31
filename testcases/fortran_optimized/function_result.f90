program function_result
  implicit none
  integer, parameter :: n = 512
  integer, parameter :: repeats = 200000
  integer :: iter
  real :: a(n)
  real :: total

  total = 0.0
  do iter = 1, repeats
    call fill_values(a, iter)
    total = total + a(1)
  end do
  print *, total + sum(a)

contains

  subroutine fill_values(out, offset)
    real, intent(out) :: out(:)
    integer, intent(in) :: offset
    integer :: i

    do i = lbound(out, 1), ubound(out, 1)
      out(i) = real(i + modulo(offset, 17))
    end do
  end subroutine fill_values

end program function_result
