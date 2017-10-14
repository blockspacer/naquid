#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

#include <arpa/inet.h>

#include "net/tools/quic/quic_client_base.h"

#include "basis/closure.h"
#include "core/nq_client_loop.h"
#include "core/nq_session.h"
#include "core/nq_config.h"

namespace net {

class QuicServerId;

class NqClient : public QuicClientBase, 
                     public QuicAlarm::Delegate,
                     public QuicCryptoClientStream::ProofHandler, 
                     public NqSession::Delegate {
 public:
  class ReconnectAlarm : public QuicAlarm::Delegate {
   public:
    ReconnectAlarm(NqClient *client) : client_(client) {}
    void OnAlarm() { client_->StartConnect(); }
   private:
    NqClient *client_;
  };
 public:
  // This will create its own QuicClientEpollNetworkHelper.
  NqClient(QuicSocketAddress server_address,
                 const QuicServerId& server_id,
                 const QuicVersionVector& supported_versions,
                 const NqClientConfig &config,
                 NqClientLoop* loop,
                 std::unique_ptr<ProofVerifier> proof_verifier);
  ~NqClient() override;

  // operation
  inline NqSession *bare_session() { return static_cast<NqSession *>(session()); }
  inline void set_destroyed() { destroyed_ = true; }

  // implements QuicClientBase. TODO(umegaya): these are really not needed?
  int GetNumSentClientHellosFromSession() override { return 0; }
  int GetNumReceivedServerConfigUpdatesFromSession() override { return 0; }
  void ResendSavedData() override {}
  void ClearDataToResend() override {}
  std::unique_ptr<QuicSession> CreateQuicClientSession(QuicConnection* connection) override;

  // implements QuicAlarm::Delegate
  void OnAlarm() override { loop_->RemoveClient(this); }

  // implements QuicCryptoClientStream::ProofHandler
  // Called when the proof in |cached| is marked valid.  If this is a secure
  // QUIC session, then this will happen only after the proof verifier
  // completes.
  void OnProofValid(const QuicCryptoClientConfig::CachedState& cached) override;

  // Called when proof verification details become available, either because
  // proof verification is complete, or when cached details are used. This
  // will only be called for secure QUIC connections.
  void OnProofVerifyDetailsAvailable(const ProofVerifyDetails& verify_details) override;


  // implements NqSession::Delegate
  void OnClose(QuicErrorCode error,
               const std::string& error_details,
               ConnectionCloseSource close_by_peer_or_self) override;
  bool OnOpen() override { return nq_closure_call(on_open_, on_conn_open, NqSession::CastFrom(this)); }
  bool IsClient() override { return bare_session()->IsClient(); }
  void Disconnect() override;
  bool Reconnect() override;
  const nq::HandlerMap *GetHandlerMap() const override;
  nq::HandlerMap *ResetHandlerMap() override;
  QuicStream* NewStream(const std::string &name) override;
  QuicCryptoStream *NewCryptoStream(NqSession *session) override;

 private:
  NqClientLoop* loop_;
  std::unique_ptr<nq::HandlerMap> own_handler_map_;
  nq_closure_t on_close_, on_open_;
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(NqClient);
};

} //net