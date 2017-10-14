#include "core/nq_client_loop.h"

#include "core/nq_client.h"

namespace net {
void NqClientLoop::Close() {
  for (auto &kv : client_map_) {
    delete kv.second;
  }
  NqLoop::Close();
}
//called from diseonnecting alarm
void NqClientLoop::RemoveClient(NqClient *cl) {
  auto it = client_map_.find(cl->server_address().ToString());
  if (it != client_map_.end()) {
    client_map_.erase(it);
  }
}
NqClient *NqClientLoop::Create(const std::string &host,
                                       int port,  
                                       NqClientConfig &config) {
  QuicServerId server_id;
  QuicSocketAddress server_address;
  if (!ParseUrl(host, port, server_id, server_address, config)) {
    return nullptr;
  }
  auto c = new NqClient(
      server_address,
      server_id, 
      AllSupportedVersions(),
      config,
      this,
      config.proof_verifier()
  );
  client_map_[server_address.ToString()] = c;
  return c;
}
/* static */
bool NqClientLoop::ParseUrl(const std::string &host, int port, QuicServerId& server_id, QuicSocketAddress &address, QuicConfig &config) {
  if (host.empty()) {
    return false;
  } else if (port == 0) {
    port = 443;
  }
  struct addrinfo filter, *resolved;
  filter.ai_socktype = SOCK_DGRAM;
  filter.ai_family = AF_UNSPEC;
  filter.ai_protocol = 0;
  filter.ai_flags = 0;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &filter, &resolved) != 0) {
    return false;
  }
  try {
    server_id = QuicServerId(host, port, PRIVACY_MODE_ENABLED); //TODO: control privacy mode from url
    address = QuicSocketAddress(*resolved->ai_addr);
    freeaddrinfo(resolved);
    return true;
  } catch (...) {
    freeaddrinfo(resolved);
    return false;
  }
}	
} //net