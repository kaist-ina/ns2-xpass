#ifndef _QUEUE_XPASS_RED_H_
#define _QUEUE_XPASS_RED_H_

#include "red.h"
#include "timer-handler.h"

class XPassRED;
class XPassREDCreditTimer: public TimerHandler {
public:
  XPassREDCreditTimer(XPassRED *a): TimerHandler(), a_(a) {}
protected:
  virtual void expire (Event *);
  XPassRED *a_;
};

class XPassRED: public REDQueue {
  friend class XPassREDCreditTimer;
public:
  XPassRED(): REDQueue(), credit_timer_(this) {
    credit_q_ = new PacketQueue;

    bind("credit_limit_", &credit_q_limit_);
    bind("max_tokens_", &max_tokens_);
    bind("token_refresh_rate_", &token_refresh_rate_);

    tokens_ = 0;
    token_bucket_clock_ = 0;
  }
  ~XPassRED() {
    delete credit_q_;
  }
protected:
  void enque(Packet *);
  Packet* deque();
  void updateTokenBucket();
  void expire();

  PacketQueue *credit_q_;
  int credit_q_limit_;
  int data_q_limit_;

  int tokens_;
  int max_tokens_;
  double token_bucket_clock_;
  double token_refresh_rate_;

  XPassREDCreditTimer credit_timer_;
};

#endif
