program allocatable_update
  implicit none
  integer, parameter :: repeats = 100000
  integer :: iter
  real, allocatable :: a(:), b(:)
  real :: total

  allocate(b(1024))
  b = 1.0
  total = 0.0
  do iter = 1, repeats
    b(1) = b(1) + 1.0e-6
    if (.not. allocated(a)) then
      allocate(a(size(b)))
    else if (size(a) /= size(b)) then
      deallocate(a)
      allocate(a(size(b)))
    end if
    a(:) = b(:)
    total = total + a(1)
  end do

  print *, total + sum(a)
end program allocatable_update
