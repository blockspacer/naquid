#include "core/nq_stream.h"

#include "basis/header_codec.h"
#include "basis/endian.h"
#include "core/nq_loop.h"
#include "core/nq_session.h"
#include "core/nq_client.h"

namespace net {

NqStream::NqStream(QuicStreamId id, NqSession* nq_session, 
                   bool establish_side, SpdyPriority priority) : 
  QuicStream(id, nq_session), 
  handler_(nullptr), 
  priority_(priority), 
  establish_side_(establish_side) {
  nq_session->RegisterStreamPriority(id, priority);
}
void NqStream::InitHandle() { 
  handle_ = nq_session()->delegate()->GetBoxer()->Box(this); 
}
NqLoop *NqStream::GetLoop() { 
  return nq_session()->delegate()->GetLoop(); 
}
NqSession *NqStream::nq_session() { 
  return static_cast<NqSession *>(session()); 
}
const NqSession *NqStream::nq_session() const { 
  return static_cast<const NqSession *>(session()); 
}
NqSessionIndex NqStream::session_index() const { 
  return nq_session()->delegate()->SessionIndex(); 
}
nq_conn_t NqStream::conn() { 
  return nq_session()->conn(); 
}
bool NqStream::set_protocol(const std::string &name) {
  if (!establish_side()) {
    ASSERT(false); //should be establish side
    return false; 
  }
  buffer_ = name;
  handler_ = std::unique_ptr<NqStreamHandler>(CreateStreamHandler(name));
  return handler_ != nullptr;
}
NqStreamHandler *NqStream::CreateStreamHandler(const std::string &name) {
  auto he = nq_session()->handler_map()->Find(name);
  if (he == nullptr) {
    ASSERT(false);
    return nullptr;
  }
  NqStreamHandler *s;
  switch (he->type) {
  case nq::HandlerMap::FACTORY: {
    s = (NqStreamHandler *)nq_closure_call(he->factory, create_stream, nq_session()->conn());
  } break;
  case nq::HandlerMap::STREAM: {
    if (nq_closure_is_empty(he->stream.stream_reader)) {
      s = new NqSimpleStreamHandler(this, he->stream.on_stream_record);
    } else {
      s = new NqRawStreamHandler(this, he->stream.on_stream_record, 
                                 he->stream.stream_reader, 
                                 he->stream.stream_writer); 
    }
    s->SetLifeCycleCallback(he->stream.on_stream_open, he->stream.on_stream_close);
  } break;
  case nq::HandlerMap::RPC: {
    s = new NqSimpleRPCStreamHandler(this, he->rpc.on_rpc_request, 
                                     he->rpc.on_rpc_notify, 
                                    he->rpc.use_large_msgid);
    s->SetLifeCycleCallback(he->rpc.on_stream_open, he->rpc.on_stream_close);
  } break;
  default:
    ASSERT(false);
    return nullptr;
  }
  return s;
}
void NqStream::Disconnect() {
  WriteOrBufferData(QuicStringPiece(), true, nullptr);
}
void NqStream::OnClose() {
  handler_->OnClose();
}
void NqStream::OnDataAvailable() {
  //greedy read and called back
  struct iovec v[256];
  int n_blocks = sequencer()->GetReadableRegions(v, 256);
  //TRACE("NqStream OnDataAvailable %u data blocks\n", n_blocks);
  int i = 0;
  if (handler_ == nullptr && !establish_side()) {
    //establishment
    for (;i < n_blocks;) {
      buffer_.append(NqStreamHandler::ToCStr(v[i].iov_base), v[i].iov_len);
      sequencer()->MarkConsumed(v[i].iov_len);
      i++;
      size_t idx = buffer_.find('\0');
      if (idx == std::string::npos) {
        continue; //not yet established
      }
      //create handler by initial establish string
      auto name = buffer_.substr(0, idx);
      handler_ = std::unique_ptr<NqStreamHandler>(CreateStreamHandler(name));
      handler_->ProtoSent();
      if (handler_ == nullptr || !handler_->OnOpen()) { //server side OnOpen
        Disconnect();
        return; //broken payload. stream handler does not exists
      }
      if (buffer_.length() > (idx + 1)) {
        //parse_buffer may contains over-received payload
        handler_->OnRecv(buffer_.c_str() + idx + 1, buffer_.length() - idx - 1);
      }
      buffer_ = std::move(name);
      break;
    }
  }
  size_t consumed = 0;
  for (;i < n_blocks; i++) {
    handler_->OnRecv(NqStreamHandler::ToCStr(v[i].iov_base), v[i].iov_len);
    consumed += v[i].iov_len;
  }
  sequencer()->MarkConsumed(consumed);  
}



void NqClientStream::OnClose() {
  auto c = static_cast<NqClient *>(nq_session()->delegate());
  c->stream_manager().OnClose(this);
  NqStream::OnClose();
}



void NqStreamHandler::WriteBytes(const char *p, nq_size_t len) {
  if (!proto_sent_) { //TODO: use unlikely
    const auto& name = stream_->protocol();
    char zero_byte = 0;
    stream_->WriteOrBufferData(QuicStringPiece(name.c_str(), name.length()), false, nullptr);
    stream_->WriteOrBufferData(QuicStringPiece(&zero_byte, 1), false, nullptr);
    OnOpen(); //client stream side OnOpen
    proto_sent_ = true;
  }
  stream_->WriteOrBufferData(QuicStringPiece(p, len), false, nullptr);
}



constexpr size_t len_buff_len = nq::LengthCodec::EncodeLength(sizeof(nq_size_t));
constexpr size_t header_buff_len = 8;
void NqSimpleStreamHandler::OnRecv(const void *p, nq_size_t len) {
  //greedy read and called back
	parse_buffer_.append(ToCStr(p), len);
	const char *pstr = parse_buffer_.c_str();
	size_t plen = parse_buffer_.length();
	nq_size_t read_ofs, reclen = nq::LengthCodec::Decode(&read_ofs, pstr, plen);
	if (reclen > 0 && (reclen + read_ofs) <= plen) {
	  nq_closure_call(on_recv_, on_stream_record, stream_->ToHandle<nq_stream_t>(), pstr + read_ofs, reclen);
	  parse_buffer_.erase(0, reclen + read_ofs);
	} else if (reclen == 0 && plen > len_buff_len) {
		//broken payload. should resolve payload length
		stream_->Disconnect();
	}
}
void NqSimpleStreamHandler::Send(const void *p, nq_size_t len) {
  QuicConnection::ScopedPacketBundler bundler(
    nq_session()->connection(), QuicConnection::SEND_ACK_IF_QUEUED);
	char buffer[len_buff_len + len];
	auto enc_len = nq::LengthCodec::Encode(len, buffer, sizeof(buffer));
  memcpy(buffer + enc_len, p, len);
	WriteBytes(buffer, enc_len + len);
}



void NqRawStreamHandler::OnRecv(const void *p, nq_size_t len) {
  int reclen;
  void *rec = nq_closure_call(reader_, stream_reader, ToCStr(p), len, &reclen);
  if (rec != nullptr) {
    nq_closure_call(on_recv_, on_stream_record, stream_->ToHandle<nq_stream_t>(), rec, reclen);
  } else if (reclen < 0) {
    stream_->Disconnect();    
  }
}
  


void NqSimpleRPCStreamHandler::EntryRequest(nq_msgid_t msgid, nq_closure_t cb, uint64_t timeout_duration_us) {
  auto req = new Request(this, msgid, cb);
  req_map_[msgid] = req;
  auto alarm = loop_->CreateAlarm(req);
  auto now = loop_->NowInUsec();
  alarm->Set(NqLoop::ToQuicTime(now + timeout_duration_us));
  //auto end = (alarm->deadline() - QuicTime::Zero()).ToMicroseconds();
  //TRACE("entry req: start %llu end %llu\n", now, end);
  req->alarm_ = alarm;
}
void NqSimpleRPCStreamHandler::OnRecv(const void *p, nq_size_t len) {
  TRACE("stream handler OnRecv %u bytes\n", len);
  //greedy read and called back
  parse_buffer_.append(ToCStr(p), len);
  //prepare tmp variables
  const char *pstr = parse_buffer_.c_str();
  size_t plen = parse_buffer_.length(), read_ofs = 0;
  int16_t type; nq_msgid_t msgid; nq_size_t reclen = 0;
  //decode header
  read_ofs = nq::HeaderCodec::Decode(&type, &msgid, pstr, plen);
  read_ofs += nq::LengthCodec::Decode(&reclen, pstr + read_ofs, plen - read_ofs);
  /* read_ofs => length of encoded header, reclen => actual payload length */
  if (reclen > 0 && (read_ofs + reclen) <= plen) {
    /*
      type > 0 && msgid != 0 => request
      type <= 0 && msgid != 0 => reply
      type > 0 && msgid == 0 => notify
    */
    pstr += read_ofs; //move pointer to top of payload
    if (msgid != 0) {
      if (type <= 0) {
        auto it = req_map_.find(msgid);
        if (it != req_map_.end()) {
          auto req = it->second;
          req_map_.erase(it);
          //reply from serve side
          nq_closure_call(req->on_data_, on_rpc_reply, stream_->ToHandle<nq_rpc_t>(), type, ToPV(pstr), reclen);
          req->alarm_->Cancel(); //cancel firing alarm
          delete req->alarm_; //it deletes req itself
        } else {
          //probably timedout. caller should already be received timeout error
          //req object deleted in OnAlarm
        }
      } else {
        //request
        nq_closure_call(on_request_, on_rpc_request, stream_->ToHandle<nq_rpc_t>(), type, msgid, ToPV(pstr), reclen);
      }
    } else if (type > 0) {
      //notify
      nq_closure_call(on_notify_, on_rpc_notify, stream_->ToHandle<nq_rpc_t>(), type, ToPV(pstr), reclen);
    } else {
      ASSERT(false);
    }
    parse_buffer_.erase(0, reclen + read_ofs);
  } else if (reclen == 0 && len > len_buff_len) {
    //broken payload. should resolve payload length
    stream_->Disconnect();
  } else {
    //TRACE("pb: %zu bytes, recv: %u bytes, reclen: %u bytes\n", plen, len, reclen);
  }
}
void NqSimpleRPCStreamHandler::Notify(uint16_t type, const void *p, nq_size_t len) {
  QuicConnection::ScopedPacketBundler bundler(
    nq_session()->connection(), QuicConnection::SEND_ACK_IF_QUEUED);
  ASSERT(type > 0);
  //pack and send buffer
  char buffer[header_buff_len + len_buff_len + len];
  size_t ofs = 0;
  ofs = nq::HeaderCodec::Encode(static_cast<int16_t>(type), 0, buffer, sizeof(buffer));
  ofs += nq::LengthCodec::Encode(len, buffer + ofs, sizeof(buffer) - ofs);
  memcpy(buffer + ofs, p, len);
  WriteBytes(buffer, ofs + len);  
}
void NqSimpleRPCStreamHandler::Send(uint16_t type, const void *p, nq_size_t len, nq_closure_t cb) {
  //QuicConnection::ScopedPacketBundler bundler(
    //nq_session()->connection(), QuicConnection::SEND_ACK_IF_QUEUED);
  ASSERT(type > 0);
  nq_msgid_t msgid = msgid_factory_.New();
  //pack and send buffer
  char buffer[header_buff_len + len_buff_len + len];
  size_t ofs = 0;
  ofs = nq::HeaderCodec::Encode(static_cast<int16_t>(type), msgid, buffer, sizeof(buffer));
  ofs += nq::LengthCodec::Encode(len, buffer + ofs, sizeof(buffer) - ofs);
  memcpy(buffer + ofs, p, len);
  WriteBytes(buffer, ofs + len);  

  EntryRequest(msgid, cb);
}
void NqSimpleRPCStreamHandler::Reply(nq_result_t result, nq_msgid_t msgid, const void *p, nq_size_t len) {
  //QuicConnection::ScopedPacketBundler bundler(
    //nq_session()->connection(), QuicConnection::SEND_ACK_IF_QUEUED);
  ASSERT(result <= 0);
  //pack and send buffer
  char buffer[header_buff_len + len_buff_len + len];
  size_t ofs = 0;
  ofs = nq::HeaderCodec::Encode(result, msgid, buffer, sizeof(buffer));
  ofs += nq::LengthCodec::Encode(len, buffer + ofs, sizeof(buffer) - ofs);
  memcpy(buffer + ofs, p, len);
  WriteBytes(buffer, ofs + len);  
}



} //net
