#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>

void benchmark_blur(cv::Mat &input) {
  auto start = std::chrono::high_resolution_clock::now();

  cv::Mat output;
  cv::GaussianBlur(input, output, cv::Size(15, 15), 0);

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> ms = end - start;
  std::cout << "OpenCV Gaussian Blur: " << ms.count() << " ms" << std::endl;
}

int main() {
  cv::Mat test_img(2160, 3840, CV_8UC3, cv::Scalar(128, 128, 128)); // 4K Image
  benchmark_blur(test_img);
  return 0;
}
