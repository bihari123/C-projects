#include <immintrin.h> // For AVX intrinsics
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Matrix structure
typedef struct {
  int rows;
  int cols;
  float *data; // Row-major order
} Matrix;

// Create a matrix with the specified dimensions
Matrix *create_matrix(int rows, int cols) {
  Matrix *mat = (Matrix *)malloc(sizeof(Matrix));
  mat->rows = rows;
  mat->cols = cols;
  // Align to 32-byte boundary for AVX operations
  mat->data = (float *)aligned_alloc(32, rows * cols * sizeof(float));
  if (!mat->data) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return mat;
}

// Initialize matrix with random values
void init_random_matrix(Matrix *mat) {
  for (int i = 0; i < mat->rows * mat->cols; i++) {
    mat->data[i] = (float)(rand() % 100) / 10.0f;
  }
}

// Initialize matrix with zeros
void init_zero_matrix(Matrix *mat) {
  memset(mat->data, 0, mat->rows * mat->cols * sizeof(float));
}

// Free matrix memory
void free_matrix(Matrix *mat) {
  free(mat->data);
  free(mat);
}

// Print matrix (for small matrices)
void print_matrix(Matrix *mat, const char *name) {
  printf("%s (%dx%d):\n", name, mat->rows, mat->cols);
  for (int i = 0; i < mat->rows; i++) {
    for (int j = 0; j < mat->cols; j++) {
      printf("%8.2f ", mat->data[i * mat->cols + j]);
    }
    printf("\n");
  }
  printf("\n");
}

// Naive scalar matrix multiplication
void matrix_multiply_scalar(Matrix *A, Matrix *B, Matrix *C) {
  // Ensure dimensions are compatible
  if (A->cols != B->rows || C->rows != A->rows || C->cols != B->cols) {
    printf("Error: Incompatible matrix dimensions for multiplication\n");
    return;
  }

  // Initialize C with zeros
  init_zero_matrix(C);

  int M = A->rows;
  int N = B->cols;
  int K = A->cols; // = B->rows

  // Perform matrix multiplication: C = A * B
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      float sum = 0.0f;
      for (int k = 0; k < K; k++) {
        sum += A->data[i * K + k] * B->data[k * N + j];
      }
      C->data[i * N + j] = sum;
    }
  }
}

// SIMD matrix multiplication using AVX
void matrix_multiply_simd(Matrix *A, Matrix *B, Matrix *C) {
  // Ensure dimensions are compatible
  if (A->cols != B->rows || C->rows != A->rows || C->cols != B->cols) {
    printf("Error: Incompatible matrix dimensions for multiplication\n");
    return;
  }

  int M = A->rows;
  int N = B->cols;
  int K = A->cols; // = B->rows

  // Initialize C with zeros
  init_zero_matrix(C);

  // We'll work with the original B matrix directly (no transposition)
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j += 8) {
      int remaining = (j + 8 <= N) ? 8 : (N - j);

      if (remaining == 8) {
        // Initialize 8 accumulators for 8 columns
        __m256 sum = _mm256_setzero_ps();

        // For each element in the ith row of A
        for (int k = 0; k < K; k++) {
          // Load the element A[i][k]
          float a_val = A->data[i * K + k];

          // Broadcast a_val to all 8 SIMD lanes
          __m256 a_vec = _mm256_set1_ps(a_val);

          // We need to collect 8 elements from the kth row of B
          // But they might not be contiguous, so we need to collect them
          // individually
          float b_row[8];
          for (int jj = 0; jj < 8; jj++) {
            b_row[jj] = B->data[k * N + (j + jj)];
          }

          // Load these collected elements
          __m256 b_vec = _mm256_loadu_ps(b_row);

          // Multiply and accumulate
          sum = _mm256_add_ps(sum, _mm256_mul_ps(a_vec, b_vec));
        }

        // Store the result in matrix C
        _mm256_storeu_ps(&C->data[i * N + j], sum);
      } else {
        // Handle the edge case (where we have fewer than 8 columns left)
        for (int jj = j; jj < N; jj++) {
          float sum = 0.0f;
          for (int k = 0; k < K; k++) {
            sum += A->data[i * K + k] * B->data[k * N + jj];
          }
          C->data[i * N + jj] = sum;
        }
      }
    }
  }
}

// Optimized SIMD matrix multiplication with blocking
void matrix_multiply_simd_blocked(Matrix *A, Matrix *B, Matrix *C) {
  // Ensure dimensions are compatible
  if (A->cols != B->rows || C->rows != A->rows || C->cols != B->cols) {
    printf("Error: Incompatible matrix dimensions for multiplication\n");
    return;
  }

  int M = A->rows;
  int N = B->cols;
  int K = A->cols; // = B->rows

  // Initialize C with zeros
  init_zero_matrix(C);

  // Block sizes tuned for L1/L2 cache
  const int BM = 32;
  const int BN = 32;
  const int BK = 32;

  // Temporary storage for a block of B transposed
  float *B_block = (float *)aligned_alloc(32, BN * BK * sizeof(float));
  if (!B_block) {
    fprintf(stderr, "Memory allocation failed for block matrix\n");
    return;
  }

  // Loop over blocks of output matrix
  for (int i0 = 0; i0 < M; i0 += BM) {
    int iLimit = (i0 + BM < M) ? i0 + BM : M;

    for (int j0 = 0; j0 < N; j0 += BN) {
      int jLimit = (j0 + BN < N) ? j0 + BN : N;

      for (int k0 = 0; k0 < K; k0 += BK) {
        int kLimit = (k0 + BK < K) ? k0 + BK : K;

        // Transpose just this block of B into temporary storage
        for (int k = k0; k < kLimit; k++) {
          for (int j = j0; j < jLimit; j++) {
            // Transposed indexing: B[k,j] -> B_block[j-j0,k-k0]
            B_block[(j - j0) * (kLimit - k0) + (k - k0)] = B->data[k * N + j];
          }
        }

        // Compute using the transposed block
        for (int i = i0; i < iLimit; i++) {
          // Process chunks of 8 columns at a time with SIMD
          for (int j = j0; j < jLimit; j += 8) {
            // Handle boundary case where we have less than 8 columns remaining
            if (j + 8 <= jLimit) {
              // Full SIMD processing (8 columns at once)
              __m256 c_values = _mm256_loadu_ps(&C->data[i * N + j]);

              for (int k = k0; k < kLimit; k++) {
                // Broadcast A[i,k] to all 8 lanes
                __m256 a_val = _mm256_set1_ps(A->data[i * K + k]);

                // Gather 8 values from the transposed block
                // These are elements B[k,j] to B[k,j+7] in original B
                float b_row[8];
                for (int jj = 0; jj < 8; jj++) {
                  // Access the transposed block at position [j+jj-j0, k-k0]
                  b_row[jj] = B_block[(j + jj - j0) * (kLimit - k0) + (k - k0)];
                }

                // Load B values and perform multiplication
                __m256 b_vals = _mm256_loadu_ps(b_row);
                c_values =
                    _mm256_add_ps(c_values, _mm256_mul_ps(a_val, b_vals));
              }

              // Store results back to C
              _mm256_storeu_ps(&C->data[i * N + j], c_values);
            } else {
              // Edge case: process remaining columns (< 8) with scalar code
              for (int jj = j; jj < jLimit; jj++) {
                for (int k = k0; k < kLimit; k++) {
                  C->data[i * N + jj] +=
                      A->data[i * K + k] * B->data[k * N + jj];
                }
              }
            }
          }
        }
      }
    }
  }

  free(B_block);
}

// Compare two matrices with detailed error reporting
int verify_results(Matrix *A, Matrix *B, const char *label) {
  if (A->rows != B->rows || A->cols != B->cols) {
    printf("Error: Cannot compare matrices of different dimensions\n");
    return 0;
  }

  int total_elements = A->rows * A->cols;
  int mismatches = 0;
  float max_diff = 0.0f;
  int max_diff_idx = -1;

  // Check for differences and collect statistics
  for (int i = 0; i < total_elements; i++) {
    float diff = fabsf(A->data[i] - B->data[i]);
    if (diff > 1e-3f) {
      mismatches++;
      if (diff > max_diff) {
        max_diff = diff;
        max_diff_idx = i;
      }
    }
  }

  // Report results
  if (mismatches == 0) {
    printf("%s: All values match within tolerance ✓\n", label);
    return 1;
  } else {
    printf("%s: Found %d mismatches out of %d elements (%.2f%%)\n", label,
           mismatches, total_elements,
           (float)mismatches / total_elements * 100.0f);

    // Print details about the largest difference
    if (max_diff_idx >= 0) {
      int row = max_diff_idx / A->cols;
      int col = max_diff_idx % A->cols;
      printf("  Largest diff at [%d,%d]: %.8f vs %.8f (diff = %.8f)\n", row,
             col, A->data[max_diff_idx], B->data[max_diff_idx], max_diff);
    }

    // Print a few sample differences
    int count = 0;
    printf("  Sample differences:\n");
    for (int i = 0; i < total_elements && count < 5; i++) {
      float diff = fabsf(A->data[i] - B->data[i]);
      if (diff > 1e-3f) {
        int row = i / A->cols;
        int col = i % A->cols;
        printf("    [%d,%d]: %.8f vs %.8f (diff = %.8f)\n", row, col,
               A->data[i], B->data[i], diff);
        count++;
      }
    }

    // Check if differences are significant
    if (max_diff > 0.1f) {
      printf(
          "  Error: Large differences detected, results are NOT equivalent\n");
      return 0;
    } else {
      printf("  Warning: Small differences detected (likely due to "
             "floating-point precision)\n");
      // Return success for small floating-point differences
      return 1;
    }
  }
}

// Run tests with a specific matrix size
void run_test(int size) {
  printf("=== Matrix multiplication test (%d x %d) ===\n\n", size, size);

  // Create matrices
  Matrix *A = create_matrix(size, size);
  Matrix *B = create_matrix(size, size);
  Matrix *C_scalar = create_matrix(size, size);
  Matrix *C_simd = create_matrix(size, size);
  Matrix *C_blocked = create_matrix(size, size);

  // Initialize A and B with random values
  // Use deterministic seed for reproducibility
  srand(42);
  init_random_matrix(A);
  init_random_matrix(B);

  // For small matrices, print them
  if (size <= 8) {
    print_matrix(A, "Matrix A");
    print_matrix(B, "Matrix B");
  }

  // Time the scalar multiplication
  printf("Running scalar multiplication...\n");
  clock_t start = clock();
  matrix_multiply_scalar(A, B, C_scalar);
  clock_t end = clock();
  double time_scalar = (double)(end - start) / CLOCKS_PER_SEC;
  printf("Scalar multiplication: %.6f seconds\n", time_scalar);

  // Time the SIMD multiplication
  printf("Running SIMD multiplication...\n");
  start = clock();
  matrix_multiply_simd(A, B, C_simd);
  end = clock();
  double time_simd = (double)(end - start) / CLOCKS_PER_SEC;
  printf("SIMD multiplication: %.6f seconds (%.2fx speedup)\n", time_simd,
         time_scalar / time_simd);

  // Time the blocked SIMD multiplication
  printf("Running blocked SIMD multiplication...\n");
  start = clock();
  matrix_multiply_simd_blocked(A, B, C_blocked);
  end = clock();
  double time_blocked = (double)(end - start) / CLOCKS_PER_SEC;
  printf("Blocked SIMD multiplication: %.6f seconds (%.2fx speedup)\n\n",
         time_blocked, time_scalar / time_blocked);

  // Verify results
  verify_results(C_scalar, C_simd, "SIMD vs Scalar");
  verify_results(C_scalar, C_blocked, "Blocked vs Scalar");
  verify_results(C_simd, C_blocked, "SIMD vs Blocked");

  // For small matrices, print the result
  if (size <= 8) {
    print_matrix(C_scalar, "Result Matrix (Scalar)");
    print_matrix(C_simd, "Result Matrix (SIMD)");
    print_matrix(C_blocked, "Result Matrix (Blocked SIMD)");
  }

  // Clean up
  free_matrix(A);
  free_matrix(B);
  free_matrix(C_scalar);
  free_matrix(C_simd);
  free_matrix(C_blocked);
}

// Run performance tests across multiple sizes
void run_performance_tests() {
  printf("=== Performance Tests ===\n\n");
  int sizes[] = {64, 128, 256, 512, 1024};
  int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

  printf(
      "Size\tScalar(s)\tSIMD(s)\tBlocked(s)\tSIMD Speedup\tBlocked Speedup\n");
  printf("---------------------------------------------------------------------"
         "--\n");

  for (int i = 0; i < num_sizes; i++) {
    int size = sizes[i];

    // Create matrices
    Matrix *A = create_matrix(size, size);
    Matrix *B = create_matrix(size, size);
    Matrix *C_scalar = create_matrix(size, size);
    Matrix *C_simd = create_matrix(size, size);
    Matrix *C_blocked = create_matrix(size, size);

    // Initialize with random values
    srand(42); // Same seed for reproducibility
    init_random_matrix(A);
    init_random_matrix(B);

    // Run scalar multiplication
    clock_t start = clock();
    matrix_multiply_scalar(A, B, C_scalar);
    clock_t end = clock();
    double time_scalar = (double)(end - start) / CLOCKS_PER_SEC;

    // Run SIMD multiplication
    start = clock();
    matrix_multiply_simd(A, B, C_simd);
    end = clock();
    double time_simd = (double)(end - start) / CLOCKS_PER_SEC;

    // Run blocked SIMD multiplication
    start = clock();
    matrix_multiply_simd_blocked(A, B, C_blocked);
    end = clock();
    double time_blocked = (double)(end - start) / CLOCKS_PER_SEC;

    // Verify results match
    int simd_ok = verify_results(C_scalar, C_simd, "");
    int blocked_ok = verify_results(C_scalar, C_blocked, "");

    // Print results
    printf("%d\t%.4f\t\t%.4f\t\t%.4f\t\t%.2fx%s\t\t%.2fx%s\n", size,
           time_scalar, time_simd, time_blocked, time_scalar / time_simd,
           simd_ok ? "" : "*", time_scalar / time_blocked,
           blocked_ok ? "" : "*");

    // Clean up
    free_matrix(A);
    free_matrix(B);
    free_matrix(C_scalar);
    free_matrix(C_simd);
    free_matrix(C_blocked);
  }

  printf("\n* Indicates result verification failed\n");
}

// Debug function to display performance with different block sizes
void test_block_sizes() {
  printf("=== Block Size Performance Test (1024x1024) ===\n\n");

  int size = 1024;
  Matrix *A = create_matrix(size, size);
  Matrix *B = create_matrix(size, size);
  Matrix *C = create_matrix(size, size);

  srand(42);
  init_random_matrix(A);
  init_random_matrix(B);

  // Get baseline scalar performance
  clock_t start = clock();
  matrix_multiply_scalar(A, B, C);
  clock_t end = clock();
  double time_scalar = (double)(end - start) / CLOCKS_PER_SEC;
  printf("Scalar: %.4f seconds\n\n", time_scalar);

  // Test basic SIMD performance
  start = clock();
  matrix_multiply_simd(A, B, C);
  end = clock();
  double time_simd = (double)(end - start) / CLOCKS_PER_SEC;
  printf("Basic SIMD: %.4f seconds (%.2fx speedup)\n\n", time_simd,
         time_scalar / time_simd);

  // Test different block sizes
  printf("Block Size\tTime(s)\tSpeedup\n");
  printf("---------------------------\n");

  int block_sizes[] = {8, 16, 32, 64, 128, 256};
  int num_sizes = sizeof(block_sizes) / sizeof(block_sizes[0]);

  for (int i = 0; i < num_sizes; i++) {
    int block_size = block_sizes[i];

    // We'll modify our block size at runtime for testing
    const int BM = block_size;
    const int BN = block_size;
    const int BK = block_size;

    // Temporary storage for a block of B transposed
    float *B_block = (float *)aligned_alloc(32, BN * BK * sizeof(float));

    // Initialize C with zeros
    init_zero_matrix(C);

    start = clock();

    // Loop over blocks of output matrix
    for (int i0 = 0; i0 < size; i0 += BM) {
      int iLimit = (i0 + BM < size) ? i0 + BM : size;

      for (int j0 = 0; j0 < size; j0 += BN) {
        int jLimit = (j0 + BN < size) ? j0 + BN : size;

        for (int k0 = 0; k0 < size; k0 += BK) {
          int kLimit = (k0 + BK < size) ? k0 + BK : size;

          // Transpose just this block of B into temporary storage
          for (int k = k0; k < kLimit; k++) {
            for (int j = j0; j < jLimit; j++) {
              B_block[(j - j0) * (kLimit - k0) + (k - k0)] =
                  B->data[k * size + j];
            }
          }

          // Compute using the transposed block
          for (int i = i0; i < iLimit; i++) {
            for (int j = j0; j < jLimit; j += 8) {
              if (j + 8 <= jLimit) {
                __m256 c_values = _mm256_loadu_ps(&C->data[i * size + j]);

                for (int k = k0; k < kLimit; k++) {
                  __m256 a_val = _mm256_set1_ps(A->data[i * size + k]);

                  float b_row[8];
                  for (int jj = 0; jj < 8; jj++) {
                    b_row[jj] =
                        B_block[(j + jj - j0) * (kLimit - k0) + (k - k0)];
                  }

                  __m256 b_vals = _mm256_loadu_ps(b_row);
                  c_values =
                      _mm256_add_ps(c_values, _mm256_mul_ps(a_val, b_vals));
                }

                _mm256_storeu_ps(&C->data[i * size + j], c_values);
              } else {
                for (int jj = j; jj < jLimit; jj++) {
                  for (int k = k0; k < kLimit; k++) {
                    C->data[i * size + jj] +=
                        A->data[i * size + k] * B->data[k * size + jj];
                  }
                }
              }
            }
          }
        }
      }
    }

    end = clock();
    double time_blocked = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%d\t\t%.4f\t%.2fx\n", block_size, time_blocked,
           time_scalar / time_blocked);

    free(B_block);
  }

  // Clean up
  free_matrix(A);
  free_matrix(B);
  free_matrix(C);
}

int main(int argc, char *argv[]) {
  // Check if we want to run block size tests
  if (argc > 1 && strcmp(argv[1], "blocks") == 0) {
    test_block_sizes();
    return 0;
  }

  // Check if we want to run performance tests
  if (argc > 1 && strcmp(argv[1], "performance") == 0) {
    run_performance_tests();
    return 0;
  }

  // Default: run single test
  int size = 1024;
  if (argc > 1) {
    size = atoi(argv[1]);
    if (size <= 0)
      size = 1024;
  }

  // Run test with specified or default size
  run_test(size);

  return 0;
}

/* Compile with:
   gcc -mavx2 -O3 -o matrix_multiply matrix_multiply.c -lm

   Run with:
   ./matrix_multiply         # Default 1024x1024 test
   ./matrix_multiply 8       # Test with 8x8 matrices (small enough to print)
   ./matrix_multiply performance  # Run performance comparison
   ./matrix_multiply blocks  # Test different block sizes
*/
