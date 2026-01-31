#include <complex>  // to handle fftw output
#include <cstdlib>

#include "blis.h"
#include "fftw3.h"
#include "kprof.hpp"

using namespace KProf;

void dgemm_kernel(KProfEvent& monitor, size_t& timer) {
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
  m = 32;
  n = 32;
  k = 32;
  rsc = 1;
  csc = m;
  rsa = 1;
  csa = m;
  rsb = 1;
  csb = k;
  c = (double*)std::malloc(m * n * sizeof(double));
  a = (double*)std::malloc(m * k * sizeof(double));
  b = (double*)std::malloc(k * n * sizeof(double));

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

  {
    auto startTime = std::chrono::high_resolution_clock::now();

    // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
    bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa,
              csa, b, rsb, csb, &beta, c, rsc, csc);
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }
  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, m, n, &two, c,
            rsc, csc);

  {
    monitor.StartCounters();
    // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
    bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa,
              csa, b, rsb, csb, &beta, c, rsc, csc);
    monitor.StopCounters();
  }

  // bli_dprintm("c: after gemm", m, n, c, rsc, csc, "% 4.3f", "");

  // Free the memory obtained via malloc().
  std::free(a);
  std::free(b);
  std::free(c);

  return;
}

void ddot_kernel(KProfEvent& monitor, size_t& timer) {
  // borrowed pretty much exactly from BLIS docs
  double* x;
  double* y;

  double z;

  // create row vectors
  dim_t n = 256;
  x = (double*)std::malloc(n * sizeof(double));
  y = (double*)std::malloc(n * sizeof(double));

  // set them to random values
  bli_drandv(n, x, 1);
  bli_drandv(n, y, 1);

  // do this, but without monitoring
  {
    auto startTime = std::chrono::high_resolution_clock::now();
    bli_ddotv(BLIS_NO_CONJUGATE, BLIS_NO_CONJUGATE, n, x, 1, y, 1, &z);
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }

  {
    monitor.StartCounters();
    // z = x \dot y
    bli_ddotv(BLIS_NO_CONJUGATE, BLIS_NO_CONJUGATE, n, x, 1, y, 1, &z);
    monitor.StopCounters();
  }
  free(x);
  free(y);

  return;
}

void fftw_kernel(KProfEvent& monitor, size_t& timer) {
  // first get a random vector

  dim_t n = 512;

  dcomplex* x = (dcomplex*)fftw_malloc(sizeof(dcomplex) * n);
  dcomplex* y = (dcomplex*)fftw_malloc(sizeof(dcomplex) * n);

  fftw_plan plan = fftw_plan_dft_1d(n, reinterpret_cast<fftw_complex*>(x),
                                    reinterpret_cast<fftw_complex*>(y),
                                    FFTW_FORWARD, FFTW_ESTIMATE);

  bli_zrandv(n, x, 1);

  {
    auto startTime = std::chrono::high_resolution_clock::now();
    fftw_execute(plan);
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }

  {
    monitor.StartCounters();
    fftw_execute(plan);
    monitor.StopCounters();
  }
  fftw_destroy_plan(plan);
  free(x);
  free(y);
  return;
}

// change this to kProfEvent
void sum_kernel(KProfEvent& monitor, size_t& timer) {
  volatile int sum = 0;
  {
    monitor.StartCounters();
    for (auto i = 0; i < 100; i++) sum += i;
    monitor.StopCounters();
  }
  sum = 0;
  {
    auto startTime = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < 100; i++) sum += i;
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }

  return;
}

void dynamic_sum_kernel(KProfEvent& monitor, size_t& timer, size_t j) {
  volatile int sum = 0;
  {
    monitor.StartCounters();
    for (auto i = 0; i < j; i++) sum += i;
    monitor.StopCounters();
  }
  sum = 0;
  {
    auto startTime = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < j; i++) sum += i;
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }

  return;
}

void dynamic_dgemm_kernel(KProfEvent& monitor, size_t& timer, size_t ndim) {
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
  m = ndim;
  n = ndim;
  k = ndim;
  rsc = 1;
  csc = m;
  rsa = 1;
  csa = m;
  rsb = 1;
  csb = k;
  c = (double*)std::malloc(m * n * sizeof(double));
  a = (double*)std::malloc(m * k * sizeof(double));
  b = (double*)std::malloc(k * n * sizeof(double));

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
  {
    auto startTime = std::chrono::high_resolution_clock::now();

    // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
    bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa,
              csa, b, rsb, csb, &beta, c, rsc, csc);
    auto stopTime = std::chrono::high_resolution_clock::now();

    timer = std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                 startTime)
                .count();
  }

  {
    monitor.StartCounters();
    // c := beta * c + alpha * a * b, where 'a', 'b', and 'c' are general
    bli_dgemm(BLIS_NO_TRANSPOSE, BLIS_NO_TRANSPOSE, m, n, k, &alpha, a, rsa,
              csa, b, rsb, csb, &beta, c, rsc, csc);
    monitor.StopCounters();
  }

  bli_dsetm(BLIS_NO_CONJUGATE, 0, BLIS_NONUNIT_DIAG, BLIS_DENSE, m, n, &two, c,
            rsc, csc);

  // bli_dprintm("c: after gemm", m, n, c, rsc, csc, "% 4.3f", "");

  // Free the memory obtained via malloc().
  std::free(a);
  std::free(b);
  std::free(c);

  return;
}