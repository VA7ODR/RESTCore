#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

int main() {
  // Minimal usage to ensure headers compile and linker pulls in needed libs
  boost::asio::io_context ioc;
  boost::beast::flat_buffer buffer;
  std::cout << "Hello, World!" << std::endl;
  return 0;
}
