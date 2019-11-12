#include "common.hpp"

#include <iostream>

#include <boost/enable_shared_from_this.hpp>

namespace {
struct FrameHeader {
  uint32_t text_size;
  uint32_t img_size;
};
static_assert(sizeof(FrameHeader) == 8, "struct size is wrong");

FrameHeader BuildFrameHeader(const Frame &frame) {
  return {std::min<uint32_t>(frame.text.size(), kMaxTextSize),
          std::min<uint32_t>(frame.img.size(), kMaxImgSize)};
}

class FrameReader : public boost::enable_shared_from_this<FrameReader> {
public:
  using Pointer = boost::shared_ptr<FrameReader>;

  static Pointer
  Create(boost::asio::ip::tcp::socket &socket, Frame &frame,
         std::function<void(const boost::system::error_code &)> done_handler) {
    return Pointer(new FrameReader(socket, frame, done_handler));
  }

  void Start() { StartReadHeader(); }

private:
  FrameReader(
      boost::asio::ip::tcp::socket &socket, Frame &frame,
      std::function<void(const boost::system::error_code &)> done_handler)
      : socket_(socket), frame_(frame), done_handler_(done_handler) {}

  boost::asio::ip::tcp::socket &socket_;
  FrameHeader frame_header_;
  Frame &frame_;
  std::function<void(const boost::system::error_code &)> done_handler_;

  void StartReadHeader() {
    frame_.img.clear();
    frame_.text.clear();
    auto self(shared_from_this());
    constexpr auto header_size = sizeof(FrameHeader);
    boost::asio::async_read(
        socket_, boost::asio::buffer(&frame_header_, header_size),
        boost::asio::transfer_exactly(header_size),
        [this, self, header_size](const boost::system::error_code &ec,
                                  std::size_t bytes_transferred) {
          if (!ec) {
            assert(bytes_transferred == header_size);
            StartReadText();
          }
        });
  }
  void StartReadText() {
    auto self(shared_from_this());
    const auto text_size =
        std::min<size_t>(frame_header_.text_size, kMaxTextSize);
    frame_.text.resize(text_size);
    boost::asio::async_read(
        socket_, boost::asio::buffer(frame_.text),
        boost::asio::transfer_exactly(text_size),
        [this, self, text_size](const boost::system::error_code &ec,
                                std::size_t bytes_transferred) {
          if (!ec) {
            assert(bytes_transferred == text_size);
            StartReadImage();
          }
        });
  }
  void StartReadImage() {
    auto self(shared_from_this());
    const auto img_size = std::min<size_t>(frame_header_.img_size, kMaxImgSize);
    frame_.img.resize(img_size);
    boost::asio::async_read(
        socket_, boost::asio::buffer(frame_.img),
        boost::asio::transfer_exactly(img_size),
        [this, self, img_size](const boost::system::error_code &ec,
                               std::size_t bytes_transferred) {
          if (!ec) {
            assert(bytes_transferred == img_size);
          }
          done_handler_(ec);
        });
  }
};
} // namespace

void AsyncSendFrame(
    boost::asio::ip::tcp::socket &socket, const Frame &frame,
    std::function<void(const boost::system::error_code &)> done_handler) {
  auto header = std::make_shared<FrameHeader>(BuildFrameHeader(frame));
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(boost::asio::buffer(header.get(), sizeof(FrameHeader)));
  buffers.push_back(boost::asio::buffer(frame.text, header->text_size));
  buffers.push_back(boost::asio::buffer(frame.img, header->img_size));
  boost::asio::async_write(
      socket, buffers,
      [header, done_handler](const boost::system::error_code &ec,
                             std::size_t bytes_transferred) {
        done_handler(ec);
      });
}

void AsyncReadFrame(
    boost::asio::ip::tcp::socket &socket, Frame &frame,
    std::function<void(const boost::system::error_code &)> done_handler) {
  auto reader = FrameReader::Create(socket, frame, done_handler);
  reader->Start();
}
