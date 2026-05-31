program function_result
  implicit none
  integer, parameter :: n = 512
  integer, parameter :: repeats = 200000
  integer :: iter
  real :: a(n)
  real :: total

  total = 0.0
  do iter = 1, repeats
    a = make_values(n, iter)
    total = total + a(1)
  end do
  print *, total + sum(a)

contains

  function make_values(count, offset) result(out)
    integer, intent(in) :: count
    integer, intent(in) :: offset
    real :: out(count)
    integer :: i

    do i = 1, count
      out(i) = real(i + modulo(offset, 17))
    end do
  end function make_values

end program function_result
