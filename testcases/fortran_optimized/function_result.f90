program function_result
  implicit none
  integer, parameter :: n = 512
  real :: a(n)

  call fill_values(a)
  print *, a(1)

contains

  subroutine fill_values(out)
    real, intent(out) :: out(:)
    integer :: i

    do i = lbound(out, 1), ubound(out, 1)
      out(i) = real(i)
    end do
  end subroutine fill_values

end program function_result
