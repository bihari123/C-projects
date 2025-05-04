/**
 * UDP Stop-and-Wait Protocol Client with File Reading Support
 *
 * This version focuses on reading files and sending their contents reliably
 * over UDP without requiring Boost.Program_Options
 */

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using boost::asio::ip::udp;

// Constants
constexpr int MAX_BUFFER_SIZE = 1024; // Max packet payload size
constexpr int MAX_RETRIES = 5;        // Maximum retransmission attempts
constexpr int TIMEOUT_MS = 1000;      // Timeout in milliseconds
constexpr uint8_t ACK_PACKET = 0xFF;  // ACK packet identifier

// Packet structure for stop-and-wait protocol
struct Packet {
  uint8_t seq_num;            // Sequence number (0 or 1 for stop-and-wait)
  uint16_t data_size;         // Size of data in bytes
  uint8_t is_last;            // Flag to indicate last packet
  char data[MAX_BUFFER_SIZE]; // Payload data

  // Calculate the total size of the packet with its header and payload
  size_t getTotalSize() const {
    return sizeof(seq_num) + sizeof(data_size) + sizeof(is_last) + data_size;
  }
};

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

    std::cout << "Starting transfer of " << data.size() << " bytes"
              << std::endl;

    prepare_next_packet();
  }

  // Prepare the next packet to be sent
  void prepare_next_packet() {
    if (bytes_sent_ >= send_data_.size()) {
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
      if (verbose_) {
        std::cout << "Received ACK for seq_num: " << (int)send_packet_.seq_num
                  << std::endl;
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
                    << std::endl;
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

// Simple help message
void print_help(const char *program_name) {
  std::cout << "UDP Stop-and-Wait File Transfer Client\n";
  std::cout << "Usage: " << program_name
            << " [options] <server_ip> <port> <filename>\n";
  std::cout << "Options:\n";
  std::cout << "  -v, --verbose    Enable verbose output\n";
  std::cout << "  -h, --help       Display this help message\n";
  std::cout << "Examples:\n";
  std::cout << "  " << program_name << " 127.0.0.1 8080 myfile.txt\n";
  std::cout << "  " << program_name
            << " --verbose 192.168.1.100 8080 largefile.bin\n";
}

int main(int argc, char *argv[]) {
  try {
    bool verbose = false;
    std::string server_ip;
    int server_port = 0;
    std::string filename;

    // Parse command line arguments manually
    if (argc < 2) {
      print_help(argv[0]);
      return 1;
    }

    // Process arguments
    int arg_index = 1;
    while (arg_index < argc) {
      std::string arg = argv[arg_index];

      if (arg == "-h" || arg == "--help") {
        print_help(argv[0]);
        return 0;
      } else if (arg == "-v" || arg == "--verbose") {
        verbose = true;
        arg_index++;
      } else {
        // Should be positional arguments
        break;
      }
    }

    // Ensure we have enough remaining positional arguments
    if (argc - arg_index < 3) {
      std::cerr << "Error: Missing required arguments\n";
      print_help(argv[0]);
      return 1;
    }

    // Get positional arguments
    server_ip = argv[arg_index++];
    server_port = std::stoi(argv[arg_index++]);
    filename = argv[arg_index++];

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

    std::cout << "File transfer complete: " << filename << " ("
              << file_data.size() << " bytes)" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
