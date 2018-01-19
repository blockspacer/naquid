#### status
- now basic client/server communication done. more test for ensure stability and performance improvement is required


#### remain tasks for 0.1.0
- [x] rpc: reduce frame number (unify length header/msgid/type to one frame)
- [x] rpc: add header byte to indicate msgid/type size
- [x] conn: client side proof verification (import ProofVerifierChromium)
- [x] conn: study is there a way to send stream name without sending it over wire (cmsghdr can do this somehow?)
  - seems cannot, we stick to send stream kind over stream for a while.
- [x] conn: changing to batch send by default, with preparing new API to flush batch buffer
- [x] conn: make user able to read property of NqSession::Delegate via nq_conn_t from all thread
- [x] conn: check ```[1122/035845.066728:WARNING:rtt_stats.cc(44)] Ignoring measured send_delta``` log is valid
- [x] conn: enable MMSG_MORE for linux
- [ ] conn: handle connectivity change (if not handled)
- [x] stream: ```nq_[stream|rpc]_task``` to access its internal property (name/ctx) safely. because these property does not assure to be access from other thread.
- [x] API: use direct pointer to access conn/stream
- [x] API: consider the good way to integrate nq_error_t and user defined error code, as the value of nq_result_t of nq_rpc_reply.
- [x] API: more API to thread safe 
- [x] API: delegate chromium log output to our callback (now LogMessage output logs by itself)
- [x] API: more detail control of rpc timeout (we providing nq_rpc_call_ex)
- [x] API: raw connection (do not use stream name to select stream protocol to be used)
- [x] API: ```nq_(conn|rpc|stream)_is_valid``` returns optional error detail msg 
- [x] API: actually obsolute APIs which are decided to obsolute (hide header declaration)
- [x] server: QuicAlarm should be more efficient
  - maybe better to more generalize NqLoop's alarm system and enable to directly call delegate object 
- [ ] server: graceful shutdown
  - stop accepting connection and wait until all request in the worker queue consumed
- [ ] client: remove stub alarm and connection helper interface
  - override NqLoop operator new/delete to avoid freeing with unique_ptr of QuicClientBase
- [ ] test: high frequent reconnection test
- [x] test: server side stream initiation
- [x] test: stream disconnection using on open callback 
- [x] test: client conn reconnection or finalization using on open callback
- [x] test: stream handle send/recv test
- [x] test: timeout test for handshake / rpc call
- [ ] test: ensure robustness for connectivity change
- [ ] bench: higher concurrency test (around 10k client connection)
- [ ] bench: ensure scalability with number of thread
- [x] bench: comparing latency and throughput with mrs, which contains ENet based gaming specific udp network library
  - ~10% faster than mrs, with 100ccu/5000 request (roughly 350k req/sec) almost batched (mrs does not allow 100+ ccu, so more comparision is not possible)


#### remain tasks for 1.0.0
- [ ] stream/rpc: unreliable packet sending (based on https://tools.ietf.org/html/draft-tiesel-quic-unreliable-streams-00)
- [ ] conn: optional faster network stack by by-passing kernel (like dpdk)


#### YAGNI
- [ ] stream/rpc: unencrypted packet sending 
- [ ] conn: more cert check. eg. optinally enable certificate transparency verification
  - maybe better just expose certificate data to user
- [ ] conn: try to use let's encrypt cert (with corresponding host name) by default


#### WIP: docs
- connection establishment
  - unified rule: connection is only active between first open callback and close callback. but object life cycle is a bit different. 
    - mainly because of the nature of mobile connection, that is, greedy reconnection is required to keep MMO-ish connectivity to the server.
  - connection object life cycle
    - client: active
      - client connection object retained over reconnection. so its possible client connection object alive but connection is not active (eg. during reconnection wait)
    - server: passive
      - server connection object is the exactly same life cycle with connection itself
      - disconnect == every resouce related with the connection is deleted. 
- stream type
  - rpc
  - stream
  - raw stream 
- stream establishment
  - client
  - server
- thread safety
