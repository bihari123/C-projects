// SIMD Intrinsics Introduction
#include <immintrin.h> // For SSE/AVX intrinsics
#include <stdio.h>

int main() {
  // Working with 128-bit SSE registers (4 floats at once)
  printf("SSE (128-bit) Example - 4 floats at once\n");
  float a_sse[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  float b_sse[4] = {5.0f, 6.0f, 7.0f, 8.0f};
  float result_sse[4];

  // Load data into SSE registers
  __m128 va = _mm_loadu_ps(a_sse);
  __m128 vb = _mm_loadu_ps(b_sse);

  // Add vectors
  __m128 sum = _mm_add_ps(va, vb);

  // Store result
  _mm_storeu_ps(result_sse, sum);

  // Print result
  printf("SSE Addition:\n");
  for (int i = 0; i < 4; i++) {
    printf("%.1f + %.1f = %.1f\n", a_sse[i], b_sse[i], result_sse[i]);
  }

  // Working with 256-bit AVX registers (8 floats at once)
  printf("\nAVX (256-bit) Example - 8 floats at once\n");
  float a_avx[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  float b_avx[8] = {9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
  float result_avx[8];

  // Load data into AVX registers
  __m256 va_avx = _mm256_loadu_ps(a_avx);
  __m256 vb_avx = _mm256_loadu_ps(b_avx);

  // Add vectors
  __m256 sum_avx = _mm256_add_ps(va_avx, vb_avx);

  // Store result
  _mm256_storeu_ps(result_avx, sum_avx);

  // Print result
  printf("AVX Addition:\n");
  for (int i = 0; i < 8; i++) {
    printf("%.1f + %.1f = %.1f\n", a_avx[i], b_avx[i], result_avx[i]);
  }

  // Demonstrate other operations
  printf("\nOther SIMD Operations:\n");

  // Multiplication
  __m256 product = _mm256_mul_ps(va_avx, vb_avx);
  _mm256_storeu_ps(result_avx, product);
  printf("Multiplication (first two elements): %.1f * %.1f = %.1f\n", a_avx[0],
         b_avx[0], result_avx[0]);

  // Minimum
  __m256 min = _mm256_min_ps(va_avx, vb_avx);
  _mm256_storeu_ps(result_avx, min);
  printf("Minimum (first two elements): min(%.1f, %.1f) = %.1f\n", a_avx[0],
         b_avx[0], result_avx[0]);

  // Maximum
  __m256 max = _mm256_max_ps(va_avx, vb_avx);
  _mm256_storeu_ps(result_avx, max);
  printf("Maximum (first two elements): max(%.1f, %.1f) = %.1f\n", a_avx[0],
         b_avx[0], result_avx[0]);

  return 0;
}

/* Compile with:
   gcc -mavx -o simd_intro ./basic_simd.c

   Common compilation flags:
   -msse4.2 : Enable SSE4.2 instructions
   -mavx : Enable AVX instructions
   -mavx2 : Enable AVX2 instructions
   -march=native : Enable all instructions supported by the current CPU
*/
