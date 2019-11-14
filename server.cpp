#include <atomic>
#include <iostream>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

#include "common.hpp"
#include "image_processor.hpp"

const Frame GetReadyFrame() {
  static Frame frame{kOkText, {}};
  return frame;
}

const Frame GetBusyFrame() {
  static Frame frame{kTooBusyText, {}};
  return frame;
}

class Session : public boost::enable_shared_from_this<Session> {
public:
  using Pointer = boost::shared_ptr<Session>;

  static Pointer Create(boost::asio::io_service &io_service,
                        std::atomic<int> &num_jobs, int max_jobs) {
    return Pointer(new Session(io_service, num_jobs, max_jobs));
  }
  boost::asio::ip::tcp::socket &Socket() { return socket_; }
  void Start() {
    if (num_jobs_.load() <= max_jobs_) {
      SendReady();
    } else {
      SendBusy();
    }
  }

  ~Session() { --num_jobs_; }

private:
  Session(boost::asio::io_service &io_service, std::atomic<int> &num_jobs,
          int max_jobs)
      : socket_(io_service), num_jobs_(num_jobs), max_jobs_(max_jobs) {
    ++num_jobs_;
  }
  void SendReady() {
    frame_ = GetReadyFrame();
    auto self(shared_from_this());
    AsyncSendFrame(socket_, frame_,
                   [this, self](const boost::system::error_code &ec) {
                     if (!ec)
                       ReadRequest();
                   });
  }

  void SendBusy() {
    frame_ = GetBusyFrame();
    auto self(shared_from_this());
    AsyncSendFrame(socket_, frame_,
                   [this, self](const boost::system::error_code & /*ec*/) {});
  }

  void ReadRequest() {
    auto self(shared_from_this());
    AsyncReadFrame(socket_, frame_,
                   [this, self](const boost::system::error_code &ec) {
                     if (ec) {
                       std::clog << "Failed to read frame\n";
                       return;
                     }
                     if (ProcessImage()) {
                       frame_.text = kOkText;
                     } else {
                       std::clog << "failed to process image\n";
                       frame_.text = kErrorText;
                       frame_.img.clear();
                     }
                     WriteResponse();
                   });
  }

  bool ProcessImage() {
    std::clog << "processing image" << std::endl;
    std::string text(frame_.text.begin(), frame_.text.end());
    return StampText(text, frame_.img);
  }

  void WriteResponse() {
    auto self(shared_from_this());
    AsyncSendFrame(
        socket_, frame_, [self](const boost::system::error_code &ec) {
          if (!ec) {
            std::clog << "response sent ok" << std::endl;
          } else {
            std::clog << "response sendign error: " << ec << std::endl;
          }
        });
  }

  boost::asio::ip::tcp::socket socket_;
  std::atomic<int> &num_jobs_;
  const int max_jobs_;
  Frame frame_;
};

struct ServerParams {
  unsigned short port{0};
  int jobs{0};
};

class Server {
public:
  Server(ServerParams &&params, boost::asio::io_service &io_service)
      : acceptor_(io_service, boost::asio::ip::tcp::endpoint(
                                  boost::asio::ip::tcp::v4(), params.port)),
        params_(params), num_jobs_(0) {
    StartAccept();
  }

private:
  void StartAccept() {
    Session::Pointer new_Session =
        Session::Create(acceptor_.get_io_service(), num_jobs_, params_.jobs);

    acceptor_.async_accept(new_Session->Socket(),
                           boost::bind(&Server::HandleAccept, this, new_Session,
                                       boost::asio::placeholders::error));
  }

  void HandleAccept(Session::Pointer new_Session,
                    const boost::system::error_code &error) {
    if (!error) {
      new_Session->Start();
    }
    StartAccept();
  }

  boost::asio::ip::tcp::acceptor acceptor_;
  ServerParams params_;
  std::atomic<int> num_jobs_;
};

bool ParseParams(int argc, char **argv, ServerParams &params) {
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()(
      "port,p", po::value<unsigned short>(&params.port)->default_value(1300),
      "port to listen")(
      "jobs,j", po::value<int>(&params.jobs)->default_value(5),
      "maximum concurrent requests")("help", "produce this help message");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  ServerParams params;
  if (!ParseParams(argc, argv, params)) {
    return 1;
  }

  try {
    boost::asio::io_service io_service;
    Server server{std::move(params), io_service};
    std::vector<std::thread> threads;
    for (int i = 0; i < params.jobs; ++i) {
      threads.emplace_back([&]() { io_service.run(); });
    }
    io_service.run();
    for(auto& thread: threads) {
      thread.join();
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
