program pointer_alias
  implicit none
  integer, parameter :: n = 512
  real, target :: backing(n)
  real :: c(n), a(n)
  real, pointer :: p(:)
  real :: total

  backing = 1.0
  c = 2.0
  p => backing

  ! Intentionally kept in array form: FIAP should treat pointer-backed
  ! expressions as alias-sensitive unless a stronger alias proof is available.
  a = p + c

  total = sum(a)
  print *, total
end program pointer_alias
