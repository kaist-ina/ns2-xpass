#ifndef _QUEUE_XPASS_DROP_TAIL_H_
#define _QUEUE_XPASS_DROP_TAIL_H_

#include "queue.h"
#include "timer-handler.h"

class XPassDropTail;
class CreditTimer: public TimerHandler {
public:
  CreditTimer(XPassDropTail *a): TimerHandler(), a_(a) {}
protected:
  virtual void expire (Event *);
  XPassDropTail *a_;
};

class XPassDropTail: public Queue {
  friend class CreditTimer;
public:
  XPassDropTail(): credit_timer_(this) {
    // Separate Queues: data queue & credit queue
    data_q_ = new PacketQueue;
    credit_q_ = new PacketQueue;

    // bind with TCL
    bind("credit_limit_", &credit_q_limit_);
    bind("data_limit_", &data_q_limit_);
    bind("max_tokens_", &max_tokens_);
    bind("token_refresh_rate_", &token_refresh_rate_);

    // Init variables
    tokens_ = 0;
    token_bucket_clock_ = 0;
  }
  ~XPassDropTail() {
    delete data_q_;
    delete credit_q_;
  }
protected:
  void enque(Packet*);
  Packet* deque();
  void updateTokenBucket();
  void expire();

  // Queue Related Varaibles
  //
  // Queue for data packets (High priority)
  PacketQueue *data_q_;
  // Queue for credit packets (Low priority)
  PacketQueue *credit_q_;
  // Maximum size of credit queue (in bytes)
  int credit_q_limit_;
  // Maximum size of data queue (in bytes)
  int data_q_limit_;

  // Token Bucket Related Varaibles
  //
  // Number of tokens remaning (in bytes)
  int tokens_;
  // Maximum number of tokens (in bytes)
  int max_tokens_;
  // Token Bucket clock
  double token_bucket_clock_;
  // Token Refresh Rate (in bytes per sec)
  // == Credit Throttling Rate
  double token_refresh_rate_;

  // Credit timer to trigger next credit transmission
  CreditTimer credit_timer_;
};

#endif
