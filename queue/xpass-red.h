#ifndef _QUEUE_XPASS_RED_H_
#define _QUEUE_XPASS_RED_H_

#include "red.h"
#include "timer-handler.h"

#define LOG_QUEUE 1
#if LOG_QUEUE
#define LOG_GRAN 0.001
#endif

class XPassRED;
class XPassREDCreditTimer: public TimerHandler {
public:
  XPassREDCreditTimer(XPassRED *a): TimerHandler(), a_(a) {}
protected:
  virtual void expire (Event *);
  XPassRED *a_;
};

#if LOG_QUEUE
class EXPQueueLogTimer : public TimerHandler {
public:
  EXPQueueLogTimer(XPassRED *a) : TimerHandler(), a_(a) {}
  virtual void expire(Event *a);
protected:
  XPassRED *a_;
};
#endif

class XPassRED: public REDQueue {
  friend class XPassREDCreditTimer;
#if LOG_QUEUE
  friend class EXPQueueLogTimer;
#endif
public:
  XPassRED(): REDQueue(), credit_timer_(this), LogTimer(this) {
    credit_q_ = new PacketQueue;

    bind("credit_limit_", &credit_q_limit_);
    bind("max_tokens_", &max_tokens_);
    bind("token_refresh_rate_", &token_refresh_rate_);
    bind("exp_id_", &exp_id_);
    bind("qidx_", &qidx_);

    tokens_ = 0;
    token_bucket_clock_ = 0;
#if LOG_QUEUE
    last_log_ = 0.0;
    last_sample_ = 0.0;
    qavg_ = 0.0;
    qmax_ = 0;
#endif
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

  int exp_id_;
#if LOG_QUEUE
  double last_log_;
  double last_sample_;
  double qavg_;
  int qmax_;
  int trace_;
  int qidx_;
  EXPQueueLogTimer LogTimer;
  void logQueue();
#endif

};

#endif
