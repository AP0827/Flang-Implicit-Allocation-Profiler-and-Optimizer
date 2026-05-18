! RUN: bbc -emit-hlfir %s -o - | fiap-opt | FileCheck %s
!
! CHECK: possibly-unnecessary
! CHECK: array expression temporary

subroutine saxpy_like(a, b, c, n)
  integer, intent(in) :: n
  real, dimension(n), intent(out) :: a
  real, dimension(n), intent(in) :: b, c

  a = b + c
end subroutine saxpy_like
