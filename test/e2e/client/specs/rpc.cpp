#include "rpc.h"

#include <memory.h>

using namespace nqtest;

static void test_ping(nq_rpc_t rpc, Test::Conn &tc) {
	auto done = tc.NewLatch(); //ok
	auto now = nq_time_now();
	char buff[sizeof(now)];
	nq::Endian::HostToNetbytes(now, buff);
	TRACE("test_ping: call RPC");
	RPC(rpc, RpcType::Ping, buff, sizeof(buff), ([rpc, done, now](
		nq_rpc_t rpc2, nq_error_t r, const void *data, nq_size_t dlen) {
		TRACE("test_ping: reply RPC");
		if (dlen != sizeof(now)) {
			done(false);
			return;
		}
		auto sent = nq::Endian::NetbytesToHost<nq_time_t>(data);
		done(nq_rpc_sid(rpc) == nq_rpc_sid(rpc2) && r >= 0 && now == sent);
	}));
}

static void test_raise(nq_rpc_t rpc, Test::Conn &tc) {
	auto done = tc.NewLatch(); //ok
	const int32_t code = -999;
	const std::string msg = "test failure reason";
	//pack payload
	char buff[sizeof(int32_t) + msg.length()];
	nq::Endian::HostToNetbytes(code, buff);
	memcpy(buff + sizeof(int32_t), msg.c_str(), msg.length());

	TRACE("test_raise: call RPC");
	RPC(rpc, RpcType::Raise, buff, sizeof(buff), ([done, code, msg](
		nq_rpc_t rpc, nq_error_t r, const void *data, nq_size_t dlen) {
		TRACE("test_raise: reply RPC");
		if (dlen != (sizeof(int32_t) + msg.length())) {
			done(false);
			return;
		}
		done(r == NQ_EUSER && 
			(nq::Endian::NetbytesToHost<int32_t>(data) == code) && 
			memcmp(msg.c_str(), static_cast<const char *>(data) + sizeof(int32_t), msg.length()) == 0);
	}));
}

static void test_close(nq_rpc_t rpc, Test::Conn &tc) {
	auto done = tc.NewLatch(); //ok
	TRACE("test_close: call RPC");
	RPC(rpc, RpcType::Close, "stream", 6, ([done](
		nq_rpc_t rpc, nq_error_t r, const void *data, nq_size_t dlen) {
		TRACE("test_close: reply RPC");
		if (r != NQ_OK || dlen != 0) {
			done(false);
			return;
		}
	}));
	WATCH_CONN(tc, ConnCloseStream, ([rpc, done](nq_rpc_t rpc2){
		if (nq_rpc_equal(rpc, rpc2)) {
			done(true);
		}
	}));
}

static void test_notify(nq_rpc_t rpc, Test::Conn &tc) {
	auto done = tc.NewLatch(); //ok
	auto done2 = tc.NewLatch(); //ok
	const std::string text = "notify this plz";
	WATCH_STREAM(tc, rpc, RpcNotify, ([done, text](
		nq_rpc_t rpc2, uint16_t type, const void *data, nq_size_t dlen) {
		TRACE("test_notify: notified");
		done(MakeString(data, dlen) == text && type == RpcType::TextNotification);
	});
	TRACE("test_notify: call RPC");
	RPC(rpc, RpcType::NotifyText, text.c_str(), text.length(), ([done2](
		nq_rpc_t, nq_error_t r, const void *data, nq_size_t dlen) {
		TRACE("test_notify: reply RPC");
		done2(r >= 0 && MakeString(data, dlen) == "notify success");
	})));
}

static void test_server_stream(nq_rpc_t rpc, const std::string &stream_name, Test::Conn &tc) {
	auto done = tc.NewLatch();	//rpc reply
	auto done2 = tc.NewLatch();	//server stream creation ok
	auto done3 = tc.NewLatch();	//server request
	const std::string text = "create stream";

	WATCH_CONN(tc, ConnOpenStream, ([&tc, rpc, done2, done3, text](nq_rpc_t rpc2, void **ppctx) {
		if (nq_rpc_sid(rpc) == nq_rpc_sid(rpc2)) {
			//this is not server stream open. should notice after 
			return true;
		} else if ((nq_rpc_sid(rpc2) % 2) != 0) {
			done2(false); //this is not server stream
			return false;
		}
		TRACE("test_server_stream: stream creation");
		void *ptr = reinterpret_cast<void *>(0x1234);
		WATCH_STREAM(tc, rpc2, RpcRequest, ([rpc2, done3, text, ptr](
			nq_rpc_t rpc3, uint16_t type, nq_msgid_t msgid, const void *data, nq_size_t dlen){
			TRACE("test_server_stream: server sent request %u", msgid);
			if (nq_rpc_sid(rpc2) != nq_rpc_sid(rpc3)) {
				done3(false);
				return;
			}
			if (nq_rpc_ctx(rpc2) != ptr) {
				done3(false);
				return;
			}
			auto reqstr = MakeString(data, dlen);
			auto reply = (std::string("from client:") + reqstr);
			nq_rpc_reply(rpc3, msgid, reply.c_str(), reply.length());
			TRACE("test_server_stream: reply to server done");
			if ((std::string("from server:") + text) == reqstr && type == RpcType::ServerRequest) {
				auto a = nq_rpc_alarm(rpc3);
				ALARM(a, nq_time_msec(100), ([done3](nq_time_t *next) {
					TRACE("alarm!");
					done3(true);
				}));
			} else {
				done3(false);
			}
		}));
		*ppctx = ptr;
		done2(true);
		return true;
	}));
	TRACE("test_server_stream: call RPC");
	std::string buffer = (stream_name + "|" + text);
	RPC(rpc, RpcType::ServerStream, buffer.c_str(), buffer.length(), ([done](
		nq_rpc_t, nq_error_t r, const void *data, nq_size_t dlen) {
		TRACE("test_server_stream: reply RPC");
		done(r >= 0 && dlen == 0);
	}));
}

void test_rpc(Test::Conn &conn) {
	conn.OpenRpc("rpc", [&conn](nq_rpc_t rpc, void **ppctx) {
		test_ping(rpc, conn);
		test_raise(rpc, conn);
		test_close(rpc, conn);
		return true;
	});
	conn.OpenRpc("rpc", [&conn](nq_rpc_t rpc, void **ppctx) {
		test_notify(rpc, conn);
		test_server_stream(rpc, "rpc", conn);
		return true;
	});
}
