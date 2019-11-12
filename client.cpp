#include <fstream>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>

#include "common.hpp"

constexpr auto kBusyTimeoutMs = 1000;

class IOError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

void FillFrameImage(Frame &frame, const char *filename) {
  std::ifstream input{filename, std::ios::binary};
  frame.img.resize(kMaxImgSize);
  input.read(reinterpret_cast<char *>(frame.img.data()), kMaxImgSize);
  if (input.gcount() >= kMaxImgSize) {
    throw IOError("Input image is too large. Limit: " +
                  std::to_string(kMaxImgSize));
  }
  if (!input.eof()) {
    throw IOError("Input image read error");
  }
  frame.img.resize(input.gcount());
}

void SaveFrameImage(const Frame &frame, const char *filename) {
  std::ofstream output{filename, std::ios::binary};
  if (!output.write(reinterpret_cast<const char *>(frame.img.data()),
                    frame.img.size())) {
    throw IOError("failed to save output");
  }
}

bool ProcessAsyncEvents(boost::asio::io_service &io_service) {
  io_service.reset();
  io_service.run();
}

int main(int argc, char *argv[]) {
  try {
    if (argc != 6) {
      std::cerr
          << "Usage: client <host> <port> <text> <input_file> <output_file>"
          << std::endl;
      return 1;
    }
    struct Params {
      const char *host;
      const char *port;
      const char *in_file;
      const char *text;
      const char *out_file;
    } params = {argv[1], argv[2], argv[3], argv[4], argv[5]};

    std::string text{params.in_file};
    Frame frame;
    {
      // Talk to server
      using boost::asio::ip::tcp;
      boost::asio::io_service io_service;

      tcp::resolver resolver(io_service);
      tcp::resolver::query query(params.host, params.port);
      tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

      for (;;) {
        tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);

        // 1. read busy status
        AsyncReadFrame(socket, frame, [](boost::system::error_code ec) {
          if (ec)
            throw IOError("Failed to read busy status");
        });
        ProcessAsyncEvents(io_service);

        if (frame.text == kTooBusyText) {
          std::cout << "Server busy, retrying in " << kBusyTimeoutMs << "ms"
                    << std::endl;
          std::this_thread::sleep_for(
              std::chrono::milliseconds(kBusyTimeoutMs));
          continue;
        } else if (frame.text != kOkText) {
          throw IOError("Unexpected response");
        }

        // 2. send text and image
        frame.text.assign(text.begin(), text.end());
        FillFrameImage(frame, params.text);
        AsyncSendFrame(socket, frame, [](boost::system::error_code ec) {
          if (ec)
            throw IOError("Failed to send image");
        });
        ProcessAsyncEvents(io_service);

        // 3. receive processed image
        AsyncReadFrame(socket, frame, [](boost::system::error_code ec) {
          if (ec)
            throw IOError("Failed to read processed image");
        });
        ProcessAsyncEvents(io_service);
        if (frame.text != kOkText) {
          throw IOError("Server failed to process image");
        }
        break;
      }
    }
    // 4. save processed image
    SaveFrameImage(frame, params.out_file);
  } catch (const IOError &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
