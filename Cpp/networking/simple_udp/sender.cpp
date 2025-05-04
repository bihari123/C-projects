/**
 * UDP Stop-and-Wait Protocol Implementation Using Boost.Asio
 * Cross-platform (Windows, Linux, macOS)
 *
 * This code demonstrates a simple reliable transmission protocol
 * built on top of UDP using the stop-and-wait approach and Boost.Asio
 * for networking functionality.
 */

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <chrono>
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

// UDP Server implementation
class UdpServer {
private:
  boost::asio::io_context &io_context_;
  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  uint8_t expected_seq_num_;
  bool is_running_;

  // Buffer for incoming data
  Packet receive_buffer_;
  std::vector<char> assembled_data_;

public:
  UdpServer(boost::asio::io_context &io_context, int port)
      : io_context_(io_context),
        socket_(io_context, udp::endpoint(udp::v4(), port)),
        expected_seq_num_(0), is_running_(true) {
    std::cout << "Server started on port " << port << std::endl;
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

  // Handle received data
  void handle_receive(const boost::system::error_code &error,
                      size_t bytes_received) {
    if (!error) {
      std::cout << "Received packet with seq_num: "
                << (int)receive_buffer_.seq_num << ", size: " << bytes_received
                << " bytes" << std::endl;

      // Check if this is the packet we're expecting
      if (receive_buffer_.seq_num == expected_seq_num_) {
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
          // Process the fully assembled data here
          process_assembled_data();
          // Reset for next transmission
          assembled_data_.clear();
        }
      } else {
        std::cout
            << "Received duplicate or out-of-order packet, expected seq_num: "
            << (int)expected_seq_num_ << std::endl;
      }

      // Send ACK regardless (handles case where ACK was lost)
      send_ack(receive_buffer_.seq_num);

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
        [seq_num](const boost::system::error_code &error,
                  std::size_t bytes_sent) {
          if (!error) {
            std::cout << "ACK sent for seq_num: " << (int)seq_num << std::endl;
          } else {
            std::cerr << "Failed to send ACK: " << error.message() << std::endl;
          }
        });
  }

  // Process the fully assembled data
  void process_assembled_data() {
    // Here you would process the complete received data
    std::cout << "Processing completed data (" << assembled_data_.size()
              << " bytes)" << std::endl;

    // Example: Convert to string and print first 100 characters
    if (!assembled_data_.empty()) {
      std::string data_str(assembled_data_.begin(), assembled_data_.end());
      std::cout << "Data preview: "
                << data_str.substr(0, std::min(data_str.size(), size_t(100)))
                << (data_str.size() > 100 ? "..." : "") << std::endl;
    }
  }

  // Stop the server
  void stop() {
    is_running_ = false;
    socket_.cancel();
  }
};

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

  // Current packet being sent
  Packet send_packet_;
  uint8_t ack_buffer_;

public:
  UdpClient(boost::asio::io_context &io_context, const std::string &server_ip,
            int server_port)
      : io_context_(io_context),
        socket_(io_context, udp::endpoint(udp::v4(), 0)), // Bind to any port
        server_endpoint_(boost::asio::ip::address::from_string(server_ip),
                         server_port),
        bytes_sent_(0), current_seq_num_(0), retry_count_(0),
        timer_(io_context) {

    // Set up socket timeout
    socket_.set_option(boost::asio::socket_base::receive_buffer_size(8192));
    socket_.set_option(boost::asio::socket_base::send_buffer_size(8192));

    // Note: Boost.Asio doesn't have a direct timeout option like
    // receive_timeout We'll handle timeouts using the timer instead

    std::cout << "Client initialized, connecting to " << server_ip << ":"
              << server_port << std::endl;
  }

  // Send data using stop-and-wait protocol
  void send_data(const std::vector<char> &data) {
    send_data_ = data;
    bytes_sent_ = 0;
    current_seq_num_ = 0;

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

    std::cout << "Sending packet with seq_num: " << (int)send_packet_.seq_num
              << " (attempt " << retry_count_ + 1 << ")" << std::endl;

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
      std::cout << "Received ACK for seq_num: " << (int)send_packet_.seq_num
                << std::endl;

      // Update bytes sent
      bytes_sent_ += send_packet_.data_size;

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

// Example usage of the server
void run_server(int port) {
  try {
    boost::asio::io_context io_context;
    UdpServer server(io_context, port);

    // Start receiving packets
    server.start_receive();

    // Run the IO context
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Server exception: " << e.what() << std::endl;
  }
}

// Example usage of the client
void run_client(const std::string &server_ip, int server_port,
                const std::string &message) {
  try {
    boost::asio::io_context io_context;
    UdpClient client(io_context, server_ip, server_port);

    // Convert string to vector of chars
    std::vector<char> data(message.begin(), message.end());

    // Send the data
    client.send_data(data);

    // Run the IO context
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Client exception: " << e.what() << std::endl;
  }
}

// Main function with examples of how to use the code
int main(int argc, char *argv[]) {
  try {
    // Check command line arguments
    if (argc < 2) {
      std::cerr << "Usage:\n";
      std::cerr << "  Server mode: " << argv[0] << " --server <port>\n";
      std::cerr << "  Client mode: " << argv[0]
                << " --client <server_ip> <port> <message>\n";
      return 1;
    }

    std::string mode = argv[1];

    if (mode == "--server") {
      if (argc != 3) {
        std::cerr << "Server mode requires port number\n";
        return 1;
      }
      int port = std::stoi(argv[2]);
      run_server(port);
    } else if (mode == "--client") {
      if (argc != 5) {
        std::cerr << "Client mode requires server IP, port, and message\n";
        return 1;
      }
      std::string server_ip = argv[2];
      int server_port = std::stoi(argv[3]);
      std::string message = argv[4];
      run_client(server_ip, server_port, message);
    } else {
      std::cerr << "Unknown mode: " << mode << "\n";
      return 1;
    }
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
