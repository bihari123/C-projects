/**
 * UDP Stop-and-Wait Protocol with Fixed CRC and Latency Measurements
 * Cross-platform (Windows, Linux, macOS)
 */

#include <algorithm>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/crc.hpp>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using boost::asio::ip::udp;
using namespace std::chrono;

// Constants
constexpr int MAX_BUFFER_SIZE = 1024; // Max packet payload size
constexpr int MAX_RETRIES = 5;        // Maximum retransmission attempts
constexpr int TIMEOUT_MS = 1000;      // Timeout in milliseconds
constexpr uint8_t ACK_PACKET = 0xFF;  // ACK packet identifier

// Latency statistics structure
struct LatencyStats {
  std::vector<double> packet_latencies;         // Latency of each packet in ms
  std::vector<double> retry_latencies;          // Latency of retries in ms
  high_resolution_clock::time_point start_time; // Start time of transfer
  high_resolution_clock::time_point end_time;   // End time of transfer
  size_t total_bytes;                           // Total bytes transferred

  LatencyStats() : total_bytes(0) {}

  // Record start of transfer
  void startTransfer() { start_time = high_resolution_clock::now(); }

  // Record end of transfer
  void endTransfer(size_t bytes) {
    end_time = high_resolution_clock::now();
    total_bytes = bytes;
  }

  // Add a packet latency measurement
  void addLatency(double latency_ms, bool is_retry) {
    packet_latencies.push_back(latency_ms);
    if (is_retry) {
      retry_latencies.push_back(latency_ms);
    }
  }

  // Get average latency
  double getAverageLatency() const {
    if (packet_latencies.empty())
      return 0.0;
    return std::accumulate(packet_latencies.begin(), packet_latencies.end(),
                           0.0) /
           packet_latencies.size();
  }

  // Get minimum latency
  double getMinLatency() const {
    if (packet_latencies.empty())
      return 0.0;
    return *std::min_element(packet_latencies.begin(), packet_latencies.end());
  }

  // Get maximum latency
  double getMaxLatency() const {
    if (packet_latencies.empty())
      return 0.0;
    return *std::max_element(packet_latencies.begin(), packet_latencies.end());
  }

  // Get median latency (useful to ignore outliers)
  double getMedianLatency() const {
    if (packet_latencies.empty())
      return 0.0;

    std::vector<double> sorted_latencies = packet_latencies;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());

    size_t n = sorted_latencies.size();
    if (n % 2 == 0) {
      return (sorted_latencies[n / 2 - 1] + sorted_latencies[n / 2]) / 2.0;
    } else {
      return sorted_latencies[n / 2];
    }
  }

  // Get retry rate
  double getRetryRate() const {
    if (packet_latencies.empty())
      return 0.0;
    return static_cast<double>(retry_latencies.size()) /
           packet_latencies.size();
  }

  // Get total transfer time in milliseconds
  double getTotalTransferTime() const {
    return duration_cast<milliseconds>(end_time - start_time).count();
  }

  // Get throughput in bytes per second
  double getThroughput() const {
    double seconds =
        duration_cast<microseconds>(end_time - start_time).count() / 1000000.0;
    if (seconds < 0.001)
      return 0.0; // Avoid division by near-zero
    return total_bytes / seconds;
  }

  // Print summary statistics
  void printStats() const {
    std::cout << "\n===== Latency and Performance Statistics =====\n";
    std::cout << "Total packets sent: " << packet_latencies.size() << std::endl;
    std::cout << "Total retries: " << retry_latencies.size() << std::endl;
    std::cout << "Retry rate: " << std::fixed << std::setprecision(2)
              << (getRetryRate() * 100) << "%" << std::endl;
    std::cout << "Average packet latency: " << std::fixed
              << std::setprecision(2) << getAverageLatency() << " ms"
              << std::endl;
    std::cout << "Median packet latency: " << std::fixed << std::setprecision(2)
              << getMedianLatency() << " ms" << std::endl;
    std::cout << "Minimum packet latency: " << std::fixed
              << std::setprecision(2) << getMinLatency() << " ms" << std::endl;
    std::cout << "Maximum packet latency: " << std::fixed
              << std::setprecision(2) << getMaxLatency() << " ms" << std::endl;
    std::cout << "Total transfer time: " << std::fixed << std::setprecision(2)
              << getTotalTransferTime() << " ms" << std::endl;
    std::cout << "Data transferred: " << total_bytes << " bytes" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2)
              << (getThroughput() / 1024) << " KB/s" << std::endl;

    // Print histogram of latencies if we have enough data
    if (packet_latencies.size() > 10) {
      printLatencyHistogram();
    }
  }

  // Print histogram of latencies
  void printLatencyHistogram() const {
    const int NUM_BINS = 10;
    std::vector<int> histogram(NUM_BINS, 0);

    // Find min and max for bin sizing
    double min_latency = getMinLatency();
    double max_latency = getMaxLatency();
    double bin_size = (max_latency - min_latency) / NUM_BINS;

    // If all latencies are the same, adjust to avoid division by zero
    if (bin_size < 0.00001) {
      bin_size = 1.0;
    }

    // Count frequencies
    for (double latency : packet_latencies) {
      int bin = std::min(NUM_BINS - 1,
                         static_cast<int>((latency - min_latency) / bin_size));
      histogram[bin]++;
    }

    // Find max count for scaling
    int max_count = *std::max_element(histogram.begin(), histogram.end());

    std::cout << "\nLatency Distribution:\n";
    for (int i = 0; i < NUM_BINS; i++) {
      double bin_start = min_latency + i * bin_size;
      double bin_end = bin_start + bin_size;

      // Print bin range
      std::cout << std::fixed << std::setprecision(1) << std::setw(6)
                << bin_start << " - " << std::setw(6) << bin_end << " ms: ";

      // Print bar
      int bar_length = static_cast<int>(60.0 * histogram[i] / max_count);
      std::cout << std::string(bar_length, '#') << " (" << histogram[i] << ")"
                << std::endl;
    }
  }
};

// Ensure consistent memory layout across platforms
#pragma pack(push, 1)
// Enhanced packet structure with CRC for data verification
struct Packet {
  uint8_t seq_num;            // Sequence number (0 or 1 for stop-and-wait)
  uint16_t data_size;         // Size of data in bytes
  uint8_t is_last;            // Flag to indicate last packet
  uint32_t crc;               // Checksum for data verification
  char data[MAX_BUFFER_SIZE]; // Payload data

  // Calculate the total size of the packet with its header and payload
  size_t getTotalSize() const {
    return sizeof(seq_num) + sizeof(data_size) + sizeof(is_last) + sizeof(crc) +
           data_size;
  }
};
#pragma pack(pop)

// Helper function to debug packet information
void debugPacket(const Packet &packet, const std::string &prefix) {
  std::cout << prefix << " - "
            << "seq_num: " << (int)packet.seq_num
            << ", data_size: " << packet.data_size
            << ", is_last: " << (int)packet.is_last << ", crc: " << packet.crc
            << std::endl;

  // Debug: Print first few bytes of data in hex
  std::cout << prefix << " - Data preview: ";
  for (size_t i = 0; i < std::min(static_cast<size_t>(10),
                                  static_cast<size_t>(packet.data_size));
       ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(packet.data[i]))
              << " ";
  }
  std::cout << std::dec << std::endl;
}

// Calculate CRC-32 checksum
uint32_t calculateCRC(const char *data, size_t length) {
  boost::crc_32_type result;
  if (data && length > 0) {
    result.process_bytes(data, length);
  }
  return result.checksum();
}

// Conversion functions for endianness
uint32_t htonl32(uint32_t value) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
         ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
#else
  return value;
#endif
}

uint32_t ntohl32(uint32_t value) {
  return htonl32(value); // The conversion is symmetric
}

// Read file contents into a vector
std::vector<char> readFileContents(const std::string &filepath) {
  std::vector<char> buffer;

  // Open the file in binary mode
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + filepath);
  }

  // Get file size by seeking to the end
  std::streamsize file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::cout << "Reading file: " << filepath << " (" << file_size << " bytes)"
            << std::endl;

  // Reserve buffer capacity
  buffer.resize(file_size);

  // Read file into buffer
  if (file_size > 0) {
    if (!file.read(buffer.data(), file_size)) {
      throw std::runtime_error("Failed to read file contents: " + filepath);
    }
  }

  file.close();
  return buffer;
}

// Save received data to a file
void saveToFile(const std::vector<char> &data, const std::string &filepath) {
  std::ofstream file(filepath, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to create output file: " + filepath);
  }

  file.write(data.data(), data.size());
  file.close();

  std::cout << "Saved " << data.size() << " bytes to file: " << filepath
            << std::endl;
}

// Compare two data buffers and report differences
bool verifyData(const std::vector<char> &original,
                const std::vector<char> &received) {
  if (original.size() != received.size()) {
    std::cout << "Data size mismatch: original=" << original.size()
              << " bytes, received=" << received.size() << " bytes"
              << std::endl;
    return false;
  }

  size_t mismatches = 0;
  for (size_t i = 0; i < original.size(); ++i) {
    if (original[i] != received[i]) {
      if (mismatches < 10) { // Only show the first 10 mismatches
        std::cout << "Data mismatch at position " << i << ": original=0x"
                  << std::hex << (int)(unsigned char)original[i]
                  << ", received=0x" << (int)(unsigned char)received[i]
                  << std::dec << std::endl;
      }
      mismatches++;
    }
  }

  if (mismatches > 0) {
    std::cout << "Total data mismatches: " << mismatches << " bytes ("
              << (mismatches * 100.0 / original.size()) << "%)" << std::endl;
    return false;
  }

  std::cout << "Data verification successful: all " << original.size()
            << " bytes match" << std::endl;
  return true;
}

// UDP Client implementation
class UdpClient {
private:
  boost::asio::io_context &io_context_;
  udp::socket socket_;
  udp::endpoint server_endpoint_;
  std::vector<char> send_data_;
  size_t bytes_sent_;
  uint8_t current_seq_num_;
  int retry_count_;
  boost::asio::steady_timer timer_;
  bool verbose_;
  LatencyStats latency_stats_;
  high_resolution_clock::time_point packet_send_time_;

  // Current packet being sent
  Packet send_packet_;
  uint8_t ack_buffer_;

public:
  UdpClient(boost::asio::io_context &io_context, const std::string &server_ip,
            int server_port, bool verbose = false)
      : io_context_(io_context),
        socket_(io_context, udp::endpoint(udp::v4(), 0)), // Bind to any port
        server_endpoint_(boost::asio::ip::address::from_string(server_ip),
                         server_port),
        bytes_sent_(0), current_seq_num_(0), retry_count_(0),
        timer_(io_context), verbose_(verbose) {

    // Set up socket buffer sizes
    socket_.set_option(boost::asio::socket_base::receive_buffer_size(8192));
    socket_.set_option(boost::asio::socket_base::send_buffer_size(8192));

    std::cout << "Client initialized, connecting to " << server_ip << ":"
              << server_port << std::endl;
  }

  // Send data using stop-and-wait protocol
  void send_data(const std::vector<char> &data) {
    send_data_ = data;
    bytes_sent_ = 0;
    current_seq_num_ = 0;

    // Start timing the transfer
    latency_stats_.startTransfer();

    std::cout << "Starting transfer of " << data.size() << " bytes"
              << std::endl;

    prepare_next_packet();
  }

  // Get latency statistics
  const LatencyStats &getLatencyStats() const { return latency_stats_; }

  // Prepare the next packet to be sent
  void prepare_next_packet() {
    if (bytes_sent_ >= send_data_.size()) {
      // Transfer complete, record end time and stats
      latency_stats_.endTransfer(send_data_.size());
      std::cout << "All data sent successfully (" << send_data_.size()
                << " bytes)" << std::endl;
      return;
    }

    // Calculate data size for this packet
    size_t remaining_bytes = send_data_.size() - bytes_sent_;
    size_t packet_data_size =
        std::min(remaining_bytes, static_cast<size_t>(MAX_BUFFER_SIZE));

    // Create packet
    send_packet_.seq_num = current_seq_num_;
    send_packet_.data_size = static_cast<uint16_t>(packet_data_size);
    send_packet_.is_last =
        (bytes_sent_ + packet_data_size == send_data_.size()) ? 1 : 0;

    // Copy data to packet
    std::memcpy(send_packet_.data, &send_data_[bytes_sent_], packet_data_size);

    // Calculate CRC (only on the actual data)
    uint32_t raw_crc = calculateCRC(send_packet_.data, packet_data_size);
    send_packet_.crc = htonl32(raw_crc); // Convert to network byte order

    if (verbose_) {
      debugPacket(send_packet_, "Preparing packet");
    }

    // Reset retry count
    retry_count_ = 0;

    // Start sending
    send_packet_with_retry();
  }

  // Send the current packet with retry mechanism
  void send_packet_with_retry() {
    if (retry_count_ >= MAX_RETRIES) {
      std::cerr << "Failed to send packet after " << MAX_RETRIES << " attempts"
                << std::endl;
      return;
    }

    // Record send time for latency measurement
    packet_send_time_ = high_resolution_clock::now();

    if (verbose_ || retry_count_ > 0) {
      std::cout << "Sending packet with seq_num: " << (int)send_packet_.seq_num
                << ", size: " << send_packet_.data_size << " bytes"
                << " (attempt " << retry_count_ + 1 << ")"
                << " [" << bytes_sent_ << "/" << send_data_.size()
                << " bytes total]" << std::endl;
    }

    // Send the packet
    socket_.async_send_to(
        boost::asio::buffer(&send_packet_, send_packet_.getTotalSize()),
        server_endpoint_,
        boost::bind(&UdpClient::handle_send, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  // Handle send completion
  void handle_send(const boost::system::error_code &error, size_t bytes_sent) {
    if (!error) {
      // Wait for ACK
      wait_for_ack();
    } else {
      std::cerr << "Send error: " << error.message() << std::endl;
      retry_count_++;

      // Schedule retry with exponential backoff
      timer_.expires_after(
          boost::asio::chrono::milliseconds(50 * (1 << retry_count_)));
      timer_.async_wait(boost::bind(&UdpClient::send_packet_with_retry, this));
    }
  }

  // Wait for acknowledgment
  void wait_for_ack() {
    socket_.async_receive_from(
        boost::asio::buffer(&ack_buffer_, sizeof(ack_buffer_)),
        server_endpoint_,
        boost::bind(&UdpClient::handle_receive_ack, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));

    // Set a timeout for ACK receipt
    timer_.expires_after(boost::asio::chrono::milliseconds(TIMEOUT_MS));
    timer_.async_wait(boost::bind(&UdpClient::handle_timeout, this,
                                  boost::asio::placeholders::error));
  }

  // Handle received ACK
  void handle_receive_ack(const boost::system::error_code &error,
                          size_t bytes_received) {
    // Cancel the timeout timer
    timer_.cancel();

    if (!error && bytes_received == sizeof(uint8_t) &&
        ack_buffer_ == ACK_PACKET) {
      // Calculate round-trip time
      auto now = high_resolution_clock::now();
      double latency_ms =
          duration_cast<microseconds>(now - packet_send_time_).count() / 1000.0;

      // Record latency statistics
      latency_stats_.addLatency(latency_ms, retry_count_ > 0);

      if (verbose_) {
        std::cout << "Received ACK for seq_num: " << (int)send_packet_.seq_num
                  << " (latency: " << std::fixed << std::setprecision(2)
                  << latency_ms << " ms)" << std::endl;
      }

      // Update bytes sent
      bytes_sent_ += send_packet_.data_size;

      // Show progress if not verbose (verbose mode already shows per-packet
      // progress)
      if (!verbose_ && send_data_.size() > MAX_BUFFER_SIZE) {
        // Only show progress every 5% or 10 packets, whichever comes first
        static size_t last_percentage = 0;
        static int packet_count = 0;

        packet_count++;
        size_t current_percentage = (bytes_sent_ * 100) / send_data_.size();

        if (current_percentage >= last_percentage + 5 || packet_count >= 10) {
          std::cout << "Progress: " << current_percentage << "% ("
                    << bytes_sent_ << "/" << send_data_.size() << " bytes)"
                    << " [latency: " << std::fixed << std::setprecision(2)
                    << latency_ms << " ms]" << std::endl;
          last_percentage = current_percentage;
          packet_count = 0;
        }
      }

      // Flip sequence number for next packet
      current_seq_num_ = 1 - current_seq_num_;

      // Move to next packet
      prepare_next_packet();
    } else if (!error) {
      std::cerr << "Received invalid ACK, retrying..." << std::endl;
      retry_count_++;
      send_packet_with_retry();
    } else if (error != boost::asio::error::operation_aborted) {
      // If error is not due to timer cancellation
      std::cerr << "ACK receive error: " << error.message() << std::endl;
      retry_count_++;
      send_packet_with_retry();
    }
  }

  // Handle timeout waiting for ACK
  void handle_timeout(const boost::system::error_code &error) {
    if (!error) { // if operation hasn't been cancelled
      std::cout << "ACK timeout, retransmitting..." << std::endl;

      // Cancel any pending receive operation
      socket_.cancel();

      // Increment retry counter and retransmit
      retry_count_++;
      send_packet_with_retry();
    }
  }
};

// UDP Server implementation
class UdpServer {
private:
  boost::asio::io_context &io_context_;
  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  uint8_t expected_seq_num_;
  bool is_running_;
  std::string output_filepath_;
  bool verbose_;
  LatencyStats latency_stats_;
  high_resolution_clock::time_point packet_receive_time_;
  high_resolution_clock::time_point transfer_start_time_;

  // Buffer for incoming data
  Packet receive_buffer_;
  std::vector<char> assembled_data_;

public:
  UdpServer(boost::asio::io_context &io_context, int port,
            std::string output_filepath = "", bool verbose = false)
      : io_context_(io_context),
        socket_(io_context, udp::endpoint(udp::v4(), port)),
        expected_seq_num_(0), is_running_(true),
        output_filepath_(output_filepath), verbose_(verbose) {

    std::cout << "Server started on port " << port << std::endl;
    if (!output_filepath_.empty()) {
      std::cout << "Data will be saved to: " << output_filepath_ << std::endl;
    }

    // Initialize latency stats
    latency_stats_.startTransfer();
    transfer_start_time_ = high_resolution_clock::now();
  }

  // Start receiving data
  void start_receive() {
    std::cout << "Waiting for data..." << std::endl;
    socket_.async_receive_from(
        boost::asio::buffer(&receive_buffer_, sizeof(receive_buffer_)),
        remote_endpoint_,
        boost::bind(&UdpServer::handle_receive, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  // Get the assembled data
  const std::vector<char> &getAssembledData() const { return assembled_data_; }

  // Get latency statistics
  const LatencyStats &getLatencyStats() const { return latency_stats_; }

  // Handle received data
  void handle_receive(const boost::system::error_code &error,
                      size_t bytes_received) {
    // Record packet receive time
    packet_receive_time_ = high_resolution_clock::now();

    // Measure processing time for this packet
    high_resolution_clock::time_point process_start_time =
        high_resolution_clock::now();
    double processing_time_ms = 0.0;

    if (!error) {
      if (verbose_) {
        debugPacket(receive_buffer_, "Received packet");
      } else {
        std::cout << "Received packet with seq_num: "
                  << (int)receive_buffer_.seq_num
                  << ", size: " << receive_buffer_.data_size << " bytes"
                  << std::endl;
      }

      // Verify CRC
      uint32_t received_crc =
          ntohl32(receive_buffer_.crc); // Convert from network byte order
      uint32_t calculated_crc =
          calculateCRC(receive_buffer_.data, receive_buffer_.data_size);
      bool crc_valid = (calculated_crc == received_crc);

      if (!crc_valid) {
        std::cout << "CRC mismatch: expected=" << received_crc
                  << ", calculated=" << calculated_crc << std::endl;

        // Debug information to diagnose CRC issue
        if (verbose_) {
          std::cout << "Data for CRC calculation:" << std::endl;
          for (size_t i = 0; i < receive_buffer_.data_size; ++i) {
            if (i % 16 == 0)
              std::cout << std::endl
                        << std::hex << std::setw(4) << std::setfill('0') << i
                        << ": ";
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(
                             receive_buffer_.data[i]))
                      << " ";
          }
          std::cout << std::dec << std::endl;
        }
      }

      // Check if this is the packet we're expecting
      if (receive_buffer_.seq_num == expected_seq_num_ && crc_valid) {
        // Process the received data
        if (receive_buffer_.data_size > 0 &&
            receive_buffer_.data_size <= MAX_BUFFER_SIZE) {
          // Copy received data to our assembled buffer
          assembled_data_.insert(assembled_data_.end(), receive_buffer_.data,
                                 receive_buffer_.data +
                                     receive_buffer_.data_size);

          std::cout << "Added " << receive_buffer_.data_size
                    << " bytes to assembled data (total: "
                    << assembled_data_.size() << " bytes)" << std::endl;

          // Flip expected sequence number for next packet (0->1, 1->0)
          expected_seq_num_ = 1 - expected_seq_num_;
        }

        // Check if this was the last packet
        if (receive_buffer_.is_last) {
          std::cout << "Last packet received, data reception complete."
                    << std::endl;

          // Record end time and stats
          latency_stats_.endTransfer(assembled_data_.size());

          // Save to file if output path was specified
          if (!output_filepath_.empty()) {
            saveToFile(assembled_data_, output_filepath_);
          }
        }
      } else if (receive_buffer_.seq_num != expected_seq_num_) {
        std::cout
            << "Received duplicate or out-of-order packet, expected seq_num: "
            << (int)expected_seq_num_ << std::endl;
      } else if (!crc_valid) {
        std::cout << "Packet with valid sequence number but invalid CRC, "
                     "requesting retransmission"
                  << std::endl;
      }

      // Calculate processing time
      processing_time_ms =
          duration_cast<microseconds>(high_resolution_clock::now() -
                                      process_start_time)
              .count() /
          1000.0;

      // Send ACK regardless (handles case where ACK was lost)
      send_ack(receive_buffer_.seq_num);

      // Record processing latency (time from packet receipt to sending ACK)
      latency_stats_.addLatency(processing_time_ms, false);

      if (verbose_) {
        std::cout << "Packet processing time: " << std::fixed
                  << std::setprecision(2) << processing_time_ms << " ms"
                  << std::endl;
      }

      // If we're still running, listen for the next packet
      if (is_running_) {
        start_receive();
      }
    } else {
      std::cerr << "Receive error: " << error.message() << std::endl;
      if (is_running_) {
        start_receive(); // Try again
      }
    }
  }

  // Send acknowledgment
  void send_ack(uint8_t seq_num) {
    uint8_t ack = ACK_PACKET;

    socket_.async_send_to(
        boost::asio::buffer(&ack, sizeof(ack)), remote_endpoint_,
        [seq_num, this](const boost::system::error_code &error,
                        std::size_t bytes_sent) {
          if (!error) {
            if (verbose_) {
              std::cout << "ACK sent for seq_num: " << (int)seq_num
                        << std::endl;
            }
          } else {
            std::cerr << "Failed to send ACK: " << error.message() << std::endl;
          }
        });
  }

  // Stop the server
  void stop() {
    // Record end time if not already done
    if (duration_cast<milliseconds>(latency_stats_.end_time -
                                    latency_stats_.start_time)
            .count() == 0) {
      latency_stats_.endTransfer(assembled_data_.size());
    }

    is_running_ = false;
    socket_.cancel();
  }
};

// Simple help message
void print_help(const char *program_name) {
  std::cout << "UDP Stop-and-Wait File Transfer with CRC Verification and "
               "Latency Measurement\n";
  std::cout << "Usage:\n";
  std::cout << "  Client mode: " << program_name
            << " --client <server_ip> <port> <filename> [options]\n";
  std::cout << "  Server mode: " << program_name
            << " --server <port> [output_file] [options]\n";
  std::cout << "  Verification mode: " << program_name
            << " --verify <original_file> <received_file>\n";
  std::cout << "Options:\n";
  std::cout
      << "  -v, --verbose    Enable verbose output with detailed debugging\n";
  std::cout << "  -h, --help       Display this help message\n";
  std::cout << "Examples:\n";
  std::cout << "  " << program_name << " --client 127.0.0.1 8080 myfile.txt\n";
  std::cout << "  " << program_name << " --server 8080 received_file.txt\n";
  std::cout << "  " << program_name << " --verify original.txt received.txt\n";
}

int main(int argc, char *argv[]) {
  try {
    bool verbose = false;

    // Check for minimum arguments
    if (argc < 2) {
      print_help(argv[0]);
      return 1;
    }

    // Get mode
    std::string mode = argv[1];

    if (mode == "-h" || mode == "--help") {
      print_help(argv[0]);
      return 0;
    }

    // Check for verbose flag (can be anywhere in arguments)
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-v" || arg == "--verbose") {
        verbose = true;
        break;
      }
    }

    // Process arguments for each mode
    if (mode == "--client") {
      if (argc < 5) {
        std::cerr
            << "Error: Client mode requires server_ip, port, and filename\n";
        print_help(argv[0]);
        return 1;
      }

      // Get client parameters
      std::string server_ip = argv[2];
      int server_port = std::stoi(argv[3]);
      std::string filename = argv[4];

      // Read the file
      std::vector<char> file_data = readFileContents(filename);

      if (file_data.empty()) {
        std::cout << "Warning: File is empty, but will still be sent."
                  << std::endl;
      }

      // Create IO context and client
      boost::asio::io_context io_context;
      UdpClient client(io_context, server_ip, server_port, verbose);

      // Send the file data
      client.send_data(file_data);

      // Run the IO context
      io_context.run();

      // Print latency statistics
      client.getLatencyStats().printStats();

      std::cout << "File transfer complete: " << filename << " ("
                << file_data.size() << " bytes)" << std::endl;
    } else if (mode == "--server") {
      if (argc < 3) {
        std::cerr << "Error: Server mode requires port number\n";
        print_help(argv[0]);
        return 1;
      }

      // Get server parameters
      int port = std::stoi(argv[2]);
      std::string output_file = (argc > 3 && argv[3][0] != '-') ? argv[3] : "";

      // Create IO context and server
      boost::asio::io_context io_context;
      UdpServer server(io_context, port, output_file, verbose);

      // Start server
      server.start_receive();

      // Run the IO context
      io_context.run();

      // Print latency statistics
      server.getLatencyStats().printStats();
    } else if (mode == "--verify") {
      if (argc < 4) {
        std::cerr
            << "Error: Verify mode requires original and received filenames\n";
        print_help(argv[0]);
        return 1;
      }

      // Get verification parameters
      std::string original_file = argv[2];
      std::string received_file = argv[3];

      // Read both files
      std::vector<char> original_data = readFileContents(original_file);
      std::vector<char> received_data = readFileContents(received_file);

      // Verify data integrity
      bool data_matches = verifyData(original_data, received_data);

      return data_matches ? 0 : 1;
    } else {
      std::cerr << "Error: Unknown mode '" << mode << "'\n";
      print_help(argv[0]);
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
