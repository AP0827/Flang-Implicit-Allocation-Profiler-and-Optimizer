program option_pricing_real_kernel
  implicit none
  integer, parameter :: n = 2048
  integer, parameter :: repeats = 80000
  integer :: i, iter
  real :: payoff(n), hedge(n), value(n)
  real :: rate, volatility, total

  payoff = 3.0
  hedge = 0.5
  rate = 0.01
  volatility = 0.2
  total = 0.0

  do iter = 1, repeats
    payoff(1) = payoff(1) + 1.0e-6
    do concurrent (i = lbound(value, 1):ubound(value, 1))
      value(i) = exp(-rate) * payoff(i) + volatility * hedge(i)
    end do
    total = total + value(1)
  end do

  print *, total + sum(value)
end program option_pricing_real_kernel
