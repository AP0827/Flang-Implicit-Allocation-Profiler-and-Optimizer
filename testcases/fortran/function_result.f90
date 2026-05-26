program function_result
  implicit none
  integer, parameter :: n = 512
  real :: a(n)

  a = make_values(n)
  print *, a(1)

contains

  function make_values(count) result(out)
    integer, intent(in) :: count
    real :: out(count)
    integer :: i

    do i = 1, count
      out(i) = real(i)
    end do
  end function make_values

end program function_result
