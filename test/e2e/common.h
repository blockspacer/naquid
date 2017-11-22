#pragma once
#include <nq.h>
#include <basis/defs.h>
#include <basis/endian.h>

#include "rpctypes.h"

#include <functional>
#include <vector>
#include <thread>
#include <string>
#include <mutex>
#include <map>
#include <condition_variable>

namespace nqtest {

class ClosureCallerBase {
 public:
  virtual ~ClosureCallerBase() {};
  virtual nq_closure_t closure() = 0;
};

class ReplyClosureCaller : public ClosureCallerBase {
 public:
  std::function<void (nq_rpc_t, nq_result_t, const void *, nq_size_t)> cb_;
 public:
  ReplyClosureCaller() : cb_() {}
  ~ReplyClosureCaller() override {}
  nq_closure_t closure() override {
    nq_closure_t clsr;
    nq_closure_init(clsr, on_rpc_reply, &ReplyClosureCaller::Call, this);
    return clsr;
  }
  static void Call(void *arg, nq_rpc_t rpc, nq_result_t r, const void *p, nq_size_t l) {
    auto pcc = (ReplyClosureCaller *)arg;
    pcc->cb_(rpc, r, p, l);
  }
};

class RpcRequestClosureCaller : public ClosureCallerBase {
 public:
  std::function<void (nq_rpc_t, uint16_t, nq_msgid_t, const void *, nq_size_t)> cb_;
 public:
  RpcRequestClosureCaller() : cb_() {}
  ~RpcRequestClosureCaller() override {}
  nq_closure_t closure() override {
    nq_closure_t clsr;
    nq_closure_init(clsr, on_rpc_request, &RpcRequestClosureCaller::Call, this);
    return clsr;
  }
  static void Call(void *arg, nq_rpc_t rpc, uint16_t type, nq_msgid_t msgid, const void *p, nq_size_t l) {
    auto pcc = (RpcRequestClosureCaller *)arg;
    pcc->cb_(rpc, type, msgid, p, l);
  }
};
class RpcNotifyClosureCaller : public ClosureCallerBase {
 public:
  std::function<void (nq_rpc_t, uint16_t, const void *, nq_size_t)> cb_;
 public:
  RpcNotifyClosureCaller() : cb_() {}
  ~RpcNotifyClosureCaller() override {}
  nq_closure_t closure() override {
    nq_closure_t clsr;
    nq_closure_init(clsr, on_rpc_notify, &RpcNotifyClosureCaller::Call, this);
    return clsr;
  }
  static void Call(void *arg, nq_rpc_t rpc, uint16_t type, const void *p, nq_size_t l) {
    auto pcc = (RpcNotifyClosureCaller *)arg;
    pcc->cb_(rpc, type, p, l);
  }
};
class StreamRecordClosureCaller : public ClosureCallerBase {
 public:
  std::function<void (nq_stream_t, const void *, nq_size_t)> cb_;
 public:
  StreamRecordClosureCaller() : cb_() {}
  ~StreamRecordClosureCaller() override {}
  nq_closure_t closure() override {
    nq_closure_t clsr;
    nq_closure_init(clsr, on_stream_record, &StreamRecordClosureCaller::Call, this);
    return clsr;
  }
  static void Call(void *arg, nq_stream_t s, const void * p, nq_size_t l) { 
    auto pcc = (StreamRecordClosureCaller *)arg;
    pcc->cb_(s, p, l);
  }  
};
class ConnOpenStreamClosureCaller : public ClosureCallerBase {
 public:
  std::function<bool (nq_rpc_t, void **)> cb_;
 public:
  ConnOpenStreamClosureCaller() : cb_() {}
  ~ConnOpenStreamClosureCaller() override {}
  nq_closure_t closure() override {
    nq_closure_t clsr;
    nq_closure_init(clsr, on_rpc_open, &ConnOpenStreamClosureCaller::Call, this);
    return clsr;
  }
  static bool Call(void *arg, nq_rpc_t rpc, void **ppctx) { 
    auto pcc = (ConnOpenStreamClosureCaller *)arg;
    return pcc->cb_(rpc, ppctx);
  }
};



class Test {
 public:
  struct Conn;
  typedef std::function<void (bool)> Latch;
  typedef std::function<void (Conn &conn)> TestProc;
  typedef std::function<void (Conn &conn)> TestInitProc;
  enum CallbackType {
    RpcNotify,
    RpcRequest,
    StreamRecord,
    ConnOpenStream,
    ConnCloseStream,
    CallbackType_Max,
  };
  struct RequestData {
    int type;
    nq_msgid_t msgid;
    std::string payload;
  };
  struct NotifyData {
    int type;
    std::string payload;
  };
  struct Conn {
   public:
    static nq_stream_t invalid_stream;
    static nq_rpc_t invalid_rpc;
    struct Stream {
      std::map<CallbackType, std::unique_ptr<ClosureCallerBase>> notifiers;
      union {
        nq_rpc_t rpc;
        nq_stream_t st;
      };
      Stream(nq_stream_t s) : notifiers() {
        st = s;
      }
      Stream(nq_rpc_t r) : notifiers() {
        rpc = r;
      }
    };
   public:
    int index;
    std::thread th;
    std::mutex mtx;
    Test *t;

    nq_conn_t c;

    std::map<nq_sid_t, Stream> streams;
    std::map<CallbackType, std::unique_ptr<ClosureCallerBase>> conn_notifiers;

    char *send_buf;
    nq_size_t send_buf_len;

    std::vector<std::string> records;
    std::vector<RequestData> requests;
    std::vector<NotifyData> notifies;
    std::condition_variable cond;

    bool should_signal;
   public:
    ~Conn() {
      if (send_buf != nullptr) {
        free(send_buf);
      }
    }
    void Signal() {
      std::unique_lock<std::mutex> lock(mtx);
      cond.notify_one();
    }
    nq_stream_t NewStream(const std::string &name) {
      for (auto &st : streams) {
        if (name == std::string(nq_stream_name(st.second.st))) {
          return st.second.st;
        }
      }
      return invalid_stream;
    }
    nq_rpc_t NewRpc(const std::string &name) {
      for (auto &st : streams) {
        if (name == std::string(nq_rpc_name(st.second.rpc))) {
          return st.second.rpc;
        }
      }
      return invalid_rpc;
    }
    void AddStream(nq_stream_t s) {
      auto sid = nq_stream_sid(s);
      if (streams.find(sid) == streams.end()) {
        streams.emplace(sid, Stream(s));
      }
    } 
    void AddStream(nq_rpc_t rpc) {
      auto sid = nq_rpc_sid(rpc);
      if (streams.find(sid) == streams.end()) {
        streams.emplace(sid, Stream(rpc));
      }
    }
    bool RemoveStream(nq_stream_t s) {
      auto it = streams.find(nq_stream_sid(s));
      if (it != streams.end()) {
        streams.erase(it);
        return true;
      }
      return false;
    }
    bool RemoveStream(nq_rpc_t rpc) {
      auto it = streams.find(nq_rpc_sid(rpc));
      if (it != streams.end()) {
        streams.erase(it);
        return true;
      }
      return false;
    }
    bool FindClosure(CallbackType type, nq_closure_t &clsr) {
      auto it = conn_notifiers.find(type);
      if (it != conn_notifiers.end()) {
        clsr = it->second->closure();
        return true;
      }
      return false;
    }
    void SetClosure(CallbackType type, ClosureCallerBase *clsr) {
      conn_notifiers[type] = std::unique_ptr<ClosureCallerBase>(clsr);
    }
    bool FindClosure(CallbackType type, nq_stream_t s, nq_closure_t &clsr) {
      auto it = streams.find(nq_stream_sid(s));
      if (it != streams.end()) {
        auto it2 = it->second.notifiers.find(type);
        if (it2 != it->second.notifiers.end()) {
          clsr = it2->second->closure();
          return true;
        }
      }
      return false;
    }
    void SetClosure(CallbackType type, nq_stream_t s, ClosureCallerBase *clsr) {
      auto it = streams.find(nq_stream_sid(s));
      if (it != streams.end()) {
        it->second.notifiers[type] = std::unique_ptr<ClosureCallerBase>(clsr);
      }
    }
    bool FindClosure(CallbackType type, nq_rpc_t rpc, nq_closure_t &clsr) {
      auto it = streams.find(nq_rpc_sid(rpc));
      if (it != streams.end()) {
        auto it2 = it->second.notifiers.find(type);
        if (it2 != it->second.notifiers.end()) {
          clsr = it2->second->closure();
          return true;
        }
      }
      return false;
    }
    void SetClosure(CallbackType type, nq_rpc_t rpc, ClosureCallerBase *clsr) {
      auto it = streams.find(nq_rpc_sid(rpc));
      if (it != streams.end()) {
        it->second.notifiers[type] = std::unique_ptr<ClosureCallerBase>(clsr);
      }
    }
    Latch NewLatch() {
      return t->NewLatch();
    }
    void Init(Test *test, nq_conn_t conn, int idx) {
      send_buf = (char *)malloc(256);
      send_buf_len = 256;
      index = idx;
      t = test;
      c = conn;
      should_signal = false;
      (t->init_ != nullptr ? t->init_ : Test::RegisterCallback)(*this);

    }
  };
 protected:
  nq::atm_int_t running_;
  nq::atm_int_t result_;
  nq::atm_int_t test_start_;
  nq::atm_int_t thread_start_;
  nq::atm_int_t closed_conn_;
  TestProc testproc_;
  TestInitProc init_;
  nq_addr_t addr_;
  int concurrency_;
 public:
  Test(const nq_addr_t &addr, TestProc tf, TestInitProc init = nullptr, int cc = 1) : 
    running_(0), result_(0), test_start_(0), thread_start_(0), closed_conn_(0), 
    testproc_(tf), init_(init), addr_(addr), concurrency_(cc) {}  
  Latch NewLatch() {
    Start();
    return std::bind(&Test::End, this, std::placeholders::_1);
  }
  bool Run(nq_time_t idle_timeout = 0, nq_time_t timeout = 0);

 protected:
  void StartThread() { thread_start_++; }
  void Start() { test_start_++; running_++; }
  void End(bool ok) { 
    ASSERT(ok);
    running_--; 
    while (true) {
            int32_t expect = result_.load();
            if (expect < 0) {
              break;
            }
            int32_t desired = ok ? 1 : -1;
            if (atomic_compare_exchange_weak(&result_, &expect, desired)) {
                return;
            }
        }
  }
  bool IsSuccess() const { return result_.load() == 1; }
  bool Finished() const { return test_start_.load() > 0 && thread_start_ == concurrency_ && running_.load() == 0; }

  static void RegisterCallback(Conn &tc);

  static bool OnConnOpen(void *arg, nq_conn_t c, nq_handshake_event_t hsev, void *now_always_null);
  static nq_time_t OnConnClose(void *arg, nq_conn_t c, nq_result_t r, const char *reason, bool closed_from_remote);
  static void OnConnFinalize(void *arg, nq_conn_t c);

  static bool OnStreamOpen(void *arg, nq_stream_t s, void **pctx);
  static void OnStreamClose(void *arg, nq_stream_t s);
  static void OnStreamRecord(void *arg, nq_stream_t s, const void *data, nq_size_t len);
  static void OnStreamRecordSimple(void *arg, nq_stream_t s, const void *data, nq_size_t len);
  static nq_size_t StreamWriter(void *arg, nq_stream_t s, const void *data, nq_size_t len, void **ppbuf);
  static void *StreamReader(void *arg, const char *data, nq_size_t dlen, int *p_reclen);

  static bool OnRpcOpen(void *arg, nq_rpc_t s, void **pctx);
  static void OnRpcClose(void *arg, nq_rpc_t s);
  static void OnRpcRequest(void *arg, nq_rpc_t rpc, uint16_t type, nq_msgid_t msgid, const void *data, nq_size_t dlen);
  static void OnRpcNotify(void *arg, nq_rpc_t rpc, uint16_t type, const void *data, nq_size_t dlen);
};

} //nqtest




static inline std::string MakeString(const void *pvoid, nq_size_t length) {
  return std::string(static_cast<const char *>(pvoid), length);
}




#define RPC(stream, type, buff, blen, callback) { \
  auto *pcc = new nqtest::ReplyClosureCaller(); \
  pcc->cb_ = callback; \
  nq_rpc_call(stream, type, buff, blen, pcc->closure()); \
}

#define WATCH_CONN(conn, type, callback) { \
  auto *pcc = new nqtest::type##ClosureCaller(); \
  pcc->cb_ = callback; \
  conn.SetClosure(nqtest::Test::CallbackType::type, pcc); \
}

#define WATCH_STREAM(conn, stream, type, callback) { \
  auto *pcc = new nqtest::type##ClosureCaller(); \
  pcc->cb_ = callback; \
  conn.SetClosure(nqtest::Test::CallbackType::type, stream, pcc); \
}


#define ALERT_AND_EXIT(msg) { \
  TRACE("test failure: %s", msg); \
  exit(1); \
}

#define CONDWAIT(conn, lock, on_awake) { \
  conn.cond.wait(lock); \
  on_awake; \
}