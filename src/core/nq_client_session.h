#pragma once

#include "core/nq_session.h"

namespace net {
class NqClientStream;
class NqClientSession : public NqSession {
 public:
  NqClientSession(QuicConnection *connection,
                  Visitor* owner,
                  Delegate* delegate,
                	const QuicConfig& config)
  : NqSession(connection, owner, delegate, config) {
    init_crypto_stream();
  }

  inline QuicCryptoClientStream *GetClientCryptoStream() {
    ASSERT(delegate()->IsClient());
    return static_cast<QuicCryptoClientStream *>(GetMutableCryptoStream());
  }
  inline const QuicCryptoClientStream *GetClientCryptoStream() const {
    ASSERT(delegate()->IsClient());
    return static_cast<const QuicCryptoClientStream *>(GetCryptoStream());
  }

  //implements QuicSession
  QuicStream* CreateIncomingDynamicStream(QuicStreamId id) override;
  QuicStream* CreateOutgoingDynamicStream() override;

  //implements QuicConnectionVisitorInterface
  bool WillingAndAbleToWrite() const override {
    auto cs = GetClientCryptoStream();
    if (cs != nullptr && !cs->encryption_established()) {
      return false;
    }
    return QuicSession::WillingAndAbleToWrite();
  }

};
}
