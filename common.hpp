#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>

const std::vector<unsigned char> kOkText = {'2', '0', '0'};
const std::vector<unsigned char> kErrorText = {'5', '0', '0'};
const std::vector<unsigned char> kTooBusyText = {'4', '2', '9'};
constexpr auto kMaxTextSize = 1024;
constexpr auto kMaxImgSize = 10 * 1024 * 1024;

struct Frame {
  std::vector<unsigned char> text;
  std::vector<unsigned char> img;
};

void AsyncSendFrame(
    boost::asio::ip::tcp::socket &socket, const Frame &frame,
    std::function<void(const boost::system::error_code &)> done_handler);

void AsyncReadFrame(
    boost::asio::ip::tcp::socket &socket, Frame &frame,
    std::function<void(const boost::system::error_code &)> done_handler);
