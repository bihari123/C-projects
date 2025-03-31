// gcc -Wall -O3 -mavx2 -msse4.2 -o image_blur image_blur.c -lm
#include <emmintrin.h> // For SSE2 intrinsics
#include <immintrin.h> // For AVX2 intrinsics
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Define STB_IMAGE_IMPLEMENTATION before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

// Define STB_IMAGE_WRITE_IMPLEMENTATION before including stb_image_write.h
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

// A structure to represent an image
typedef struct {
  int width;
  int height;
  int channels;
  unsigned char *data;
} Image;

// Function to load an image (supports JPEG, PNG, BMP, etc.)
Image loadImage(const char *filename) {
  Image img = {0};

  // Load the image using stb_image
  img.data = stbi_load(filename, &img.width, &img.height, &img.channels, 0);

  if (!img.data) {
    printf("Error: Could not load image %s\n", filename);
    printf("Reason: %s\n", stbi_failure_reason());
  }

  return img;
}

// Function to save an image (supports JPEG, PNG, BMP, etc.)
void saveImage(const char *filename, Image img) {
  // Get file extension to determine format
  const char *dot = strrchr(filename, '.');
  if (!dot) {
    printf("Error: No file extension in %s\n", filename);
    return;
  }

  // Save the image using stb_image_write
  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
    stbi_write_jpg(filename, img.width, img.height, img.channels, img.data,
                   90); // 90% quality
  } else if (strcmp(dot, ".png") == 0) {
    stbi_write_png(filename, img.width, img.height, img.channels, img.data,
                   img.width * img.channels);
  } else if (strcmp(dot, ".bmp") == 0) {
    stbi_write_bmp(filename, img.width, img.height, img.channels, img.data);
  } else {
    printf("Error: Unsupported file format %s\n", dot);
  }
}

// Function to apply grayscale filter without SIMD
void applyGrayscaleFilter(Image img) {
  if (img.channels < 3) {
    printf("Image already grayscale or has insufficient channels\n");
    return;
  }

  for (int i = 0; i < img.width * img.height; i++) {
    int idx = i * img.channels;
    unsigned char r = img.data[idx + 0];
    unsigned char g = img.data[idx + 1];
    unsigned char b = img.data[idx + 2];

    // Convert to grayscale using weighted values
    unsigned char gray = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);

    // Set RGB channels to grayscale value (preserve alpha if it exists)
    img.data[idx + 0] = gray; // R
    img.data[idx + 1] = gray; // G
    img.data[idx + 2] = gray; // B
  }
}

// Function to apply grayscale filter using SSE2
void applyGrayscaleFilter_SSE(Image img) {
  if (img.channels < 3) {
    printf("Image already grayscale or has insufficient channels\n");
    return;
  }

  // Constants for RGB to grayscale conversion
  __m128 weight_r = _mm_set1_ps(0.299f);
  __m128 weight_g = _mm_set1_ps(0.587f);
  __m128 weight_b = _mm_set1_ps(0.114f);

  // Process 4 pixels at a time
  const int pixelsPerIteration = 4;
  int totalPixels = img.width * img.height;
  int vectorizedSize = (totalPixels / pixelsPerIteration) * pixelsPerIteration;

  // Allocate aligned memory for temporary storage
  float *pixels_r = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 16);
  float *pixels_g = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 16);
  float *pixels_b = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 16);
  float *gray_values =
      (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 16);

  for (int i = 0; i < vectorizedSize; i += pixelsPerIteration) {
    // Extract RGB values for 4 pixels
    for (int j = 0; j < pixelsPerIteration; j++) {
      int pixelIdx = (i + j) * img.channels;
      pixels_r[j] = (float)img.data[pixelIdx + 0]; // R
      pixels_g[j] = (float)img.data[pixelIdx + 1]; // G
      pixels_b[j] = (float)img.data[pixelIdx + 2]; // B
    }

    // Load pixel values into SSE vectors
    __m128 vec_r = _mm_load_ps(pixels_r);
    __m128 vec_g = _mm_load_ps(pixels_g);
    __m128 vec_b = _mm_load_ps(pixels_b);

    // Compute grayscale: r*0.299 + g*0.587 + b*0.114
    __m128 vec_gray = _mm_add_ps(
        _mm_mul_ps(vec_r, weight_r),
        _mm_add_ps(_mm_mul_ps(vec_g, weight_g), _mm_mul_ps(vec_b, weight_b)));

    // Store the result
    _mm_store_ps(gray_values, vec_gray);

    // Write back to image
    for (int j = 0; j < pixelsPerIteration; j++) {
      int pixelIdx = (i + j) * img.channels;
      unsigned char gray = (unsigned char)gray_values[j];
      img.data[pixelIdx + 0] = gray; // R
      img.data[pixelIdx + 1] = gray; // G
      img.data[pixelIdx + 2] = gray; // B
    }
  }

  // Process remaining pixels
  for (int i = vectorizedSize; i < totalPixels; i++) {
    int idx = i * img.channels;
    unsigned char r = img.data[idx + 0];
    unsigned char g = img.data[idx + 1];
    unsigned char b = img.data[idx + 2];

    unsigned char gray = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);

    img.data[idx + 0] = gray; // R
    img.data[idx + 1] = gray; // G
    img.data[idx + 2] = gray; // B
  }

  // Free aligned memory
  _mm_free(pixels_r);
  _mm_free(pixels_g);
  _mm_free(pixels_b);
  _mm_free(gray_values);
}

// Function to apply grayscale filter using AVX2
void applyGrayscaleFilter_AVX(Image img) {
  if (img.channels < 3) {
    printf("Image already grayscale or has insufficient channels\n");
    return;
  }

  // Constants for RGB to grayscale conversion
  __m256 weight_r = _mm256_set1_ps(0.299f);
  __m256 weight_g = _mm256_set1_ps(0.587f);
  __m256 weight_b = _mm256_set1_ps(0.114f);

  // Process 8 pixels at a time
  const int pixelsPerIteration = 8;
  int totalPixels = img.width * img.height;
  int vectorizedSize = (totalPixels / pixelsPerIteration) * pixelsPerIteration;

  // Allocate aligned memory for temporary storage
  float *pixels_r = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 32);
  float *pixels_g = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 32);
  float *pixels_b = (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 32);
  float *gray_values =
      (float *)_mm_malloc(pixelsPerIteration * sizeof(float), 32);

  for (int i = 0; i < vectorizedSize; i += pixelsPerIteration) {
    // Extract RGB values for 8 pixels
    for (int j = 0; j < pixelsPerIteration; j++) {
      int pixelIdx = (i + j) * img.channels;
      pixels_r[j] = (float)img.data[pixelIdx + 0]; // R
      pixels_g[j] = (float)img.data[pixelIdx + 1]; // G
      pixels_b[j] = (float)img.data[pixelIdx + 2]; // B
    }

    // Load pixel values into AVX vectors
    __m256 vec_r = _mm256_load_ps(pixels_r);
    __m256 vec_g = _mm256_load_ps(pixels_g);
    __m256 vec_b = _mm256_load_ps(pixels_b);

    // Compute grayscale: r*0.299 + g*0.587 + b*0.114
    __m256 vec_gray =
        _mm256_add_ps(_mm256_mul_ps(vec_r, weight_r),
                      _mm256_add_ps(_mm256_mul_ps(vec_g, weight_g),
                                    _mm256_mul_ps(vec_b, weight_b)));

    // Store the result
    _mm256_store_ps(gray_values, vec_gray);

    // Write back to image
    for (int j = 0; j < pixelsPerIteration; j++) {
      int pixelIdx = (i + j) * img.channels;
      unsigned char gray = (unsigned char)gray_values[j];
      img.data[pixelIdx + 0] = gray; // R
      img.data[pixelIdx + 1] = gray; // G
      img.data[pixelIdx + 2] = gray; // B
    }
  }

  // Process remaining pixels
  for (int i = vectorizedSize; i < totalPixels; i++) {
    int idx = i * img.channels;
    unsigned char r = img.data[idx + 0];
    unsigned char g = img.data[idx + 1];
    unsigned char b = img.data[idx + 2];

    unsigned char gray = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);

    img.data[idx + 0] = gray; // R
    img.data[idx + 1] = gray; // G
    img.data[idx + 2] = gray; // B
  }

  // Free aligned memory
  _mm_free(pixels_r);
  _mm_free(pixels_g);
  _mm_free(pixels_b);
  _mm_free(gray_values);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: %s <image.jpg/png/bmp>\n", argv[0]);
    return 1;
  }

  // Load the image
  Image img = loadImage(argv[1]);
  if (!img.data) {
    printf("Failed to load image\n");
    return 1;
  }

  printf("Image loaded: %dx%d with %d channels\n", img.width, img.height,
         img.channels);

  // Get file prefix (without extension)
  char prefix[256] = {0};
  const char *dot = strrchr(argv[1], '.');
  if (dot) {
    size_t prefix_len = dot - argv[1];
    strncpy(prefix, argv[1], prefix_len);
  } else {
    strcpy(prefix, argv[1]);
  }

  // Create copies for all three algorithms
  Image img_regular = {0};
  Image img_sse = {0};
  Image img_avx = {0};

  // Copy the image for regular processing
  img_regular.width = img.width;
  img_regular.height = img.height;
  img_regular.channels = img.channels;
  img_regular.data =
      (unsigned char *)malloc(img.width * img.height * img.channels);
  memcpy(img_regular.data, img.data, img.width * img.height * img.channels);

  // Copy the image for SSE processing
  img_sse.width = img.width;
  img_sse.height = img.height;
  img_sse.channels = img.channels;
  img_sse.data = (unsigned char *)malloc(img.width * img.height * img.channels);
  memcpy(img_sse.data, img.data, img.width * img.height * img.channels);

  // Copy the image for AVX processing
  img_avx.width = img.width;
  img_avx.height = img.height;
  img_avx.channels = img.channels;
  img_avx.data = (unsigned char *)malloc(img.width * img.height * img.channels);
  memcpy(img_avx.data, img.data, img.width * img.height * img.channels);

  // Measure time for regular implementation
  clock_t start_regular = clock();
  applyGrayscaleFilter(img_regular);
  clock_t end_regular = clock();
  double time_regular =
      ((double)(end_regular - start_regular)) / CLOCKS_PER_SEC;

  // Measure time for SSE implementation
  clock_t start_sse = clock();
  applyGrayscaleFilter_SSE(img_sse);
  clock_t end_sse = clock();
  double time_sse = ((double)(end_sse - start_sse)) / CLOCKS_PER_SEC;

  // Measure time for AVX implementation
  clock_t start_avx = clock();
  applyGrayscaleFilter_AVX(img_avx);
  clock_t end_avx = clock();
  double time_avx = ((double)(end_avx - start_avx)) / CLOCKS_PER_SEC;

  // Create output filenames
  char output_regular[512] = {0};
  char output_sse[512] = {0};
  char output_avx[512] = {0};

  // Append extensions based on input file type
  if (strstr(argv[1], ".jpg") || strstr(argv[1], ".jpeg")) {
    sprintf(output_regular, "%s_regular.jpg", prefix);
    sprintf(output_sse, "%s_sse.jpg", prefix);
    sprintf(output_avx, "%s_avx.jpg", prefix);
  } else if (strstr(argv[1], ".png")) {
    sprintf(output_regular, "%s_regular.png", prefix);
    sprintf(output_sse, "%s_sse.png", prefix);
    sprintf(output_avx, "%s_avx.png", prefix);
  } else {
    sprintf(output_regular, "%s_regular.bmp", prefix);
    sprintf(output_sse, "%s_sse.bmp", prefix);
    sprintf(output_avx, "%s_avx.bmp", prefix);
  }

  // Save the processed images
  saveImage(output_regular, img_regular);
  saveImage(output_sse, img_sse);
  saveImage(output_avx, img_avx);

  // Free memory
  stbi_image_free(img.data);
  free(img_regular.data);
  free(img_sse.data);
  free(img_avx.data);

  // Print timing information
  printf("Regular implementation: %.6f seconds\n", time_regular);
  printf("SSE implementation: %.6f seconds\n", time_sse);
  printf("AVX implementation: %.6f seconds\n", time_avx);
  printf("SSE Speedup: %.2fx\n", time_regular / time_sse);
  printf("AVX Speedup: %.2fx\n", time_regular / time_avx);

  return 0;
}
