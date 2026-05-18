program allocatable_update
  implicit none
  real, allocatable :: a(:), b(:)

  allocate(b(1024))
  b = 1.0
  if (.not. allocated(a)) then
    allocate(a(size(b)))
  else if (size(a) /= size(b)) then
    deallocate(a)
    allocate(a(size(b)))
  end if
  a(:) = b(:)

  print *, size(a)
end program allocatable_update
