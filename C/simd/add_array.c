#include <immintrin.h> // For AVX/SSE intrinsics
#include <stdio.h>
#include <time.h> // For timing functions

// This function adds two arrays using scalar operations
void add_arrays_scalar(float *a, float *b, float *result, int size) {
  for (int i = 0; i < size; i++) {
    result[i] = a[i] + b[i];
  }
}

// This function adds two arrays using SIMD operations
void add_arrays_simd(float *a, float *b, float *result, int size) {
  // Process 8 elements at a time using AVX
  int i = 0;
  for (; i <= size - 8; i += 8) {
    // Load 8 floats from arrays a and b
    __m256 va = _mm256_loadu_ps(&a[i]);
    __m256 vb = _mm256_loadu_ps(&b[i]);

    // Add the two vectors
    __m256 vresult = _mm256_add_ps(va, vb);

    // Store the result
    _mm256_storeu_ps(&result[i], vresult);
  }

  // Handle any remaining elements
  for (; i < size; i++) {
    result[i] = a[i] + b[i];
  }
}

int main() {
  // Create test arrays
  const int SIZE = 1000000000; // Much larger array for timing test
  float *a = (float *)aligned_alloc(
      32, SIZE * sizeof(float)); // 32-byte aligned for AVX
  float *b = (float *)aligned_alloc(32, SIZE * sizeof(float));
  float *result_scalar = (float *)aligned_alloc(32, SIZE * sizeof(float));
  float *result_simd = (float *)aligned_alloc(32, SIZE * sizeof(float));

  if (!a || !b || !result_scalar || !result_simd) {
    printf("Memory allocation failed\n");
    return 1;
  }

  // Initialize arrays with test data
  for (int i = 0; i < SIZE; i++) {
    a[i] = (float)i;
    b[i] = (float)(i * 2);
  }

  // Time the scalar addition
  clock_t start_scalar = clock();
  add_arrays_scalar(a, b, result_scalar, SIZE);
  clock_t end_scalar = clock();
  double time_scalar = (double)(end_scalar - start_scalar) / CLOCKS_PER_SEC;

  // Time the SIMD addition
  clock_t start_simd = clock();
  add_arrays_simd(a, b, result_simd, SIZE);
  clock_t end_simd = clock();
  double time_simd = (double)(end_simd - start_simd) / CLOCKS_PER_SEC;

  // Verify results match
  int mismatch = 0;
  for (int i = 0; i < SIZE; i++) {
    if (result_scalar[i] != result_simd[i]) {
      mismatch++;
    }
  }

  // Print timing results
  printf("Array size: %d elements\n", SIZE);
  printf("Scalar implementation: %.6f seconds\n", time_scalar);
  printf("SIMD implementation: %.6f seconds\n", time_simd);
  printf("Speedup: %.2fx\n", time_scalar / time_simd);
  printf("Result verification: %s\n", mismatch ? "FAILED" : "PASSED");

  // Print small sample of results for verification
  printf("\nSample Results (first 8 elements):\n");
  printf("Index\tA\tB\tScalar\tSIMD\n");
  printf("-----------------------------------\n");
  for (int i = 0; i < 8; i++) {
    printf("%d\t%.1f\t%.1f\t%.1f\t%.1f\n", i, a[i], b[i], result_scalar[i],
           result_simd[i]);
  }

  // Free allocated memory
  free(a);
  free(b);
  free(result_scalar);
  free(result_simd);

  return 0;
}
