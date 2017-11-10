#pragma once

#include <atomic>

#include "basis/defs.h"

namespace nq {
template <typename NUMBER>
class MsgIdFactory {
  std::atomic<NUMBER> seed_;
  NUMBER limit_;
  //0xFFFFFF....
  static const NUMBER kLimit = 
    (((NUMBER)0x80) << (8 * (sizeof(NUMBER) - 1))) + 
    ((((NUMBER)0x80) << (8 * (sizeof(NUMBER) - 1))) - 100);
 public:
  MsgIdFactory() : seed_(0), limit_(kLimit) {}
  void set_limit(NUMBER limit) { limit_ = limit; }

  NUMBER New() {
    while (true) {
      NUMBER expect = seed_;
      NUMBER desired = expect + 1;
      if (desired >= limit_) {
        desired = 1;
      }
      if (atomic_compare_exchange_weak(&seed_, &expect, desired)) {
        return desired;
      }
	}
    ASSERT(false);
    return 0;		
  }
};
}