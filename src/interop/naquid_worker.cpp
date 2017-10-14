#include "interop/naquid_worker.h"

#include "interop/naquid_client_loop.h"
#include "interop/naquid_dispatcher.h"
#include "interop/naquid_server_session.h"
#include "interop/naquid_server.h"

namespace net {
void NaquidWorker::Process(NaquidPacket *p) {
  for (size_t i = 0; i < dispatchers_.size(); i++) {
    if (dispatchers_[i].first == p->port()) {
      dispatchers_[i].second->Process(p);
      return;
    }
  }
  ASSERT(false);
}
void NaquidWorker::Run(PacketQueue &queue) {
  if (!Listen()) {
    return;
  }
  NaquidPacket *p;
  while (server_.alive()) {
    //consume queue
    while (queue.try_dequeue(p)) {
      //pass packet to corresponding session
      Process(p);
    }
    //wait incoming event
    loop_.Poll();
  }
  //last consume queue
  //TODO(iyatomi): packet from another worker may dropped.
  //somehow checking all the thread breaks main loop, before entering this last loop
  while (queue.try_dequeue(p)) {
    //pass packet to corresponding session
    Process(p);
  }
}
bool NaquidWorker::Listen() {
  for (auto &kv : server_.port_configs()) {
    //TODO(iyatomi): enable multiport server
    QuicSocketAddress address;
    if (!ToSocketAddress(kv.second.address_, address)) {
      ASSERT(false);
      return false;
    }
    auto listen_fd = CreateUDPSocketAndBind(address);
    if (listen_fd < 0) {
      ASSERT(false);
      return false;
    }
    auto d = new NaquidDispatcher(kv.first, kv.second, *this);
    if (loop_.Add(listen_fd, d, NaquidLoop::EV_READ | NaquidLoop::EV_WRITE) != NQ_OK) {
      nq::Syscall::Close(listen_fd);
      delete d;
      ASSERT(false);
      return false;
    }
    dispatchers_.push_back(std::pair<int, NaquidDispatcher*>(kv.first, d));
  }
  return true;
}
//helper
nq::Fd NaquidWorker::CreateUDPSocketAndBind(const QuicSocketAddress& address) {
  nq::Fd fd = QuicSocketUtils::CreateUDPSocket(address, &overflow_supported_);
  if (fd < 0) {
    QUIC_LOG(ERROR) << "CreateSocket() failed: " << strerror(errno);
    return -1;
  }

  sockaddr_storage addr = address.generic_address();
  int rc = bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (rc < 0) {
    QUIC_LOG(ERROR) << "Bind failed: " << strerror(errno);
    return -1;
  }
  QUIC_LOG(INFO) << "Listening on " << address.ToString();
  return fd; 
}
/* static */
bool NaquidWorker::ToSocketAddress(const nq_addr_t &addr, QuicSocketAddress &socket_address) {
  QuicServerId server_id;
  QuicConfig config;
  return NaquidClientLoop::ParseUrl(addr.host, addr.port, server_id, socket_address, config);
}
}
