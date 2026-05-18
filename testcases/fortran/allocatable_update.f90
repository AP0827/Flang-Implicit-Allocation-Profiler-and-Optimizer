program allocatable_update
  implicit none
  real, allocatable :: a(:), b(:)

  allocate(b(1024))
  b = 1.0
  a = b

  print *, size(a)
end program allocatable_update
