#include <nq.h>
#include "rpc.h"
#include "stream.h"
#include "timeout.h"
#include "reconnect.h"
#include "task.h"

using namespace nqtest;

#if defined(STORE_DETAIL)
extern bool is_conn_opened(uint64_t cid) {
  return true;
}
extern bool is_packet_received(uint64_t cid) {
  return true;
}
#endif



void test_suites(const nq_addr_t &addr, bool skip = true) {
  TRACE("==================== test_rpc ====================");
  {
    Test t(addr, test_rpc);
    if (!t.Run()) { ALERT_AND_EXIT("test_rpc fails"); }
  }//*/
  TRACE("==================== test_task ====================");
  {
    Test t(addr, test_task);
    if (!t.Run()) { ALERT_AND_EXIT("test_task fails"); }
  }//*/
  TRACE("==================== test_stream ====================");
  {
    Test t(addr, test_stream);
    if (!t.Run()) { ALERT_AND_EXIT("test_stream fails"); }
  }//*/
  TRACE("==================== test_raw_stream ====================");
  {
    Test::RunOptions o;
    o.raw_mode = true;

    auto tmp = addr;
    tmp.port = 28443;

    Test t(tmp, test_stream);
    if (!t.Run(&o)) { ALERT_AND_EXIT("test_raw_stream fails"); }
  }//*/
  TRACE("==================== test_timeout ====================");
  {
    Test::RunOptions o;
    o.rpc_timeout = nq_time_sec(2);
    o.idle_timeout = nq_time_sec(5);
    o.handshake_timeout = nq_time_sec(5);

    Test t(addr, test_timeout);
    if (!t.Run(&o)) { ALERT_AND_EXIT("test_timeout fails"); }
  }//*/
  TRACE("==================== test_reconnect_client ====================");
  {
    Test t(addr, test_reconnect_client);
    if (!t.Run()) { ALERT_AND_EXIT("test_reconnect_client fails"); }
  }//*/
  TRACE("==================== test_reconnect_server ====================");
  {
    auto tmp = addr;
    tmp.port = 18443;
    Test t(tmp, test_reconnect_server);
    if (!t.Run()) { ALERT_AND_EXIT("test_reconnect_server fails"); }
  }//*/
  /*{
    auto tmp = addr;
    tmp.port = 18443;
    Test t(addr, test_reconnect_stress, 64);
    if (!t.Run()) { ALERT_AND_EXIT("test_reconnect_stress fails"); }
  }//*/
}

int main(int argc, char *argv[]){
  nq_addr_t a1;
  a1.host = "127.0.0.1";
  a1.port = 8443;
  test_suites(a1);

  return 0;
}
