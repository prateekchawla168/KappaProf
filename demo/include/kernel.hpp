#include <cstdlib>

#include "blis.h"
#include "profiler.hpp"

void kernel(PerfEvent& monitor) {
  // "borrowed" with minimal mods from BLIS typed API examples

  volatile dim_t m, n, k;
  inc_t rsa, csa;
  inc_t rsb, csb;
  inc_t rsc, csc;

  double* a;
  double* b;
  double* c;
  double alpha, beta;

  // Initialize some basic constants.
  double zero = 0.0;
  double one = 1.0;
  double two = 2.0;

  // Create some matrix and vector operands to work with.
  m = 1024;
  n = 1024;
  k = 1024;
  rsc = 1;
  csc = m;
  rsa = 1;
  csa = m;
  rsb = 1;
  csb = k;
  c = (double*)std::aligned_alloc(256, m * n * sizeof(double));
  a = (double*)std::aligned_alloc(256, m * k * sizeof(double));
  b = (double*)std::aligned_alloc(256, k * n * sizeof(double));

  // Set the scalars to use.
  alpha = 0.707;
  beta = 0.707;

  // Initialize the matrix operands.
  bli_drandm(0, BLIS_DENSE, m, k, a, rsa, csa);
  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, k, n, &one, b,
            rsb, csb);
  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, m, n, &two, c,
            rsc, csc);

  // bli_dprintm("a: randomized", m, k, a, rsa, csa, "% 4.3f", "");
  // bli_dprintm("b: set to 1.0", k, n, b, rsb, csb, "% 4.3f", "");
  // bli_dprintm("c: initial value", m, n, c, rsc, csc, "% 4.3f", "");

  monitor.StartCounters();
  // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
  bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa, csa,
            b, rsb, csb, &beta, c, rsc, csc);
  monitor.StopCounters();

  // bli_dprintm("c: after gemm", m, n, c, rsc, csc, "% 4.3f", "");

  // Free the memory obtained via malloc().
  std::free(a);
  std::free(b);
  std::free(c);

  return;
}

void kernel(long long int& timer) {
  // "borrowed" with minimal mods from BLIS typed API examples

  volatile dim_t m, n, k;
  inc_t rsa, csa;
  inc_t rsb, csb;
  inc_t rsc, csc;

  double* a;
  double* b;
  double* c;
  double alpha, beta;

  // Initialize some basic constants.
  double zero = 0.0;
  double one = 1.0;
  double two = 2.0;

  // Create some matrix and vector operands to work with.
  m = 1024;
  n = 1024;
  k = 1024;
  rsc = 1;
  csc = m;
  rsa = 1;
  csa = m;
  rsb = 1;
  csb = k;
  c = (double*)std::aligned_alloc(256, m * n * sizeof(double));
  a = (double*)std::aligned_alloc(256, m * k * sizeof(double));
  b = (double*)std::aligned_alloc(256, k * n * sizeof(double));

  // Set the scalars to use.
  alpha = 0.707;
  beta = 0.707;

  // Initialize the matrix operands.
  bli_drandm(0, BLIS_DENSE, m, k, a, rsa, csa);
  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, k, n, &one, b,
            rsb, csb);
  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, m, n, &two, c,
            rsc, csc);

  // bli_dprintm("a: randomized", m, k, a, rsa, csa, "% 4.3f", "");
  // bli_dprintm("b: set to 1.0", k, n, b, rsb, csb, "% 4.3f", "");
  // bli_dprintm("c: initial value", m, n, c, rsc, csc, "% 4.3f", "");
  auto startTime = std::chrono::high_resolution_clock::now();

  // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
  bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa, csa,
            b, rsb, csb, &beta, c, rsc, csc);
  auto stopTime = std::chrono::high_resolution_clock::now();

  timer =
      std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime - startTime)
          .count();

  // bli_dprintm("c: after gemm", m, n, c, rsc, csc, "% 4.3f", "");

  // Free the memory obtained via malloc().
  std::free(a);
  std::free(b);
  std::free(c);

  return;
}

void sum(PerfEvent& monitor) {
  volatile int sum = 0;
  monitor.StartCounters();
  for (auto i = 0; i < 100; i++) sum += i;
  monitor.StopCounters();
  return;
}
