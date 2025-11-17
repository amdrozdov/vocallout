#pragma once
#include "bricks/net/tcp/tcp.h"
#include "bricks/net/tcp/impl/posix.h"

using namespace current::net;
// This implementation will be added to Current framework,
// FIXME: delete this file after the merge in C5T and replace SocketWithTimeout usage

timeval default_timeout = {1,0};


// POSIX allows numeric ports, as well as strings like "http".
template <typename T>
inline current::net::Connection SocketWithTimeout(const std::string& host,
                                                  T port_or_serv,
                                                  timeval& read_timeout=default_timeout,
                                                  timeval& write_timeout=default_timeout) {
  class ClientSocket final : public SocketHandle {
   public:
    explicit ClientSocket(const std::string& host,
                          const std::string& serv,
                          NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
                          timeval& read_timeout=default_timeout,
                          timeval& write_timeout=default_timeout)
        : SocketHandle(SocketHandle::DoNotBind(), nagle_algorithm_policy) {
      CURRENT_BRICKS_NET_LOG("S%05d ", static_cast<SOCKET>(socket));
      // Deliberately left non-const because of possible Windows issues. -- M.Z.
      auto addr_info = GetAddrInfo(host, serv);
      struct sockaddr* p_addr = addr_info->ai_addr;
      struct sockaddr_in* p_addr_in = reinterpret_cast<struct sockaddr_in*>(p_addr);
      remote_ip_and_port.ip = InetAddrToString(&(p_addr_in->sin_addr));
      remote_ip_and_port.port = htons(p_addr_in->sin_port);

      CURRENT_BRICKS_NET_LOG("S%05d connect() ...\n", static_cast<SOCKET>(socket));

      setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
      setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &write_timeout, sizeof(write_timeout));

      const int retval = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval) {
        CURRENT_THROW(SocketConnectException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
      }

#ifndef CURRENT_WINDOWS
      socklen_t addr_client_length = sizeof(sockaddr_in);
#else
      int addr_client_length = sizeof(sockaddr_in);
#endif  // CURRENT_WINDOWS
      sockaddr_in addr_client;
      if (::getsockname(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length) != 0) {
        CURRENT_THROW(SocketGetSockNameException());
      }
      local_ip_and_port.ip = InetAddrToString(&addr_client.sin_addr);
      local_ip_and_port.port = htons(addr_client.sin_port);

      CURRENT_BRICKS_NET_LOG("S%05d connect() OK\n", static_cast<SOCKET>(socket));
    }
    IPAndPort local_ip_and_port;
    IPAndPort remote_ip_and_port;
  };
  auto client_socket = ClientSocket(host, std::to_string(port_or_serv), kDefaultNagleAlgorithmPolicy, read_timeout, write_timeout);
  IPAndPort local_ip_and_port(std::move(client_socket.local_ip_and_port));
  IPAndPort remote_ip_and_port(std::move(client_socket.remote_ip_and_port));
  return Connection(std::move(client_socket), std::move(local_ip_and_port), std::move(remote_ip_and_port));
}
