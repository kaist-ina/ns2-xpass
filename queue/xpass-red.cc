#include "xpass-red.h"
#include <algorithm>

#define MIN(A,B) ((A<B)?A:B)

static class XPassREDClass: public TclClass {
public:
  XPassREDClass(): TclClass("Queue/XPassRED") {}
  TclObject* create(int, const char*const*) {
    return (new XPassRED);
  }
} class_xpass_red;

void XPassREDCreditTimer::expire (Event *) {
  a_->expire();
}

void XPassRED::expire() {
  Packet *p;
  double now = Scheduler::instance().clock();

  if (!blocked_) {
    p = deque();
    if (p) {
      utilUpdate(last_change_, now, blocked_);
      last_change_ = now;
      blocked_ = 1;
      target_->recv(p, &qh_);
    }
  }
}

void XPassRED::updateTokenBucket() {
  double now = Scheduler::instance().clock();
  double elapsed_time = now - token_bucket_clock_;
  int64_t new_tokens;

  if (elapsed_time <= 0.0) {
    return;
  }

  new_tokens = (int64_t)(elapsed_time * token_refresh_rate_);
  tokens_ += new_tokens;
  tokens_ = min(tokens_, max_tokens_);

  token_bucket_clock_ += new_tokens / token_refresh_rate_;
}

void XPassRED::enque(Packet *p) {
  if (p == NULL) {
    return;
  }
  hdr_cmn* cmnh = hdr_cmn::access(p);

  if (cmnh->ptype() == PT_XPASS_CREDIT) {
    credit_q_->enque(p);
    if (credit_q_->byteLength() > credit_q_limit_) {
      credit_q_->remove(p);
      drop(p);
    }
  } else {
    REDQueue::enque(p);
  }

#if LOG_QUEUE
  if (trace_) {
    double now = Scheduler::instance().clock();
    int qlen = q_->byteLength() + credit_q_->byteLength();
    if (qlen > qmax_) {
      qmax_ = qlen;
    }
    qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
    last_sample_ = now;
  }
#endif
}

Packet* XPassRED::deque() {
  Packet* packet = NULL;

  credit_timer_.force_cancel();
  updateTokenBucket();

  packet = credit_q_->head();
  int pkt_size = packet?hdr_cmn::access(packet)->size():0;

  if (packet && tokens_ >= pkt_size) {
    packet = credit_q_->deque();
    tokens_ -= pkt_size;
#if LOG_QUEUE
    if (trace_) {
      double now = Scheduler::instance().clock();
      int qlen = credit_q_->byteLength() + q_->byteLength();
      qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
      last_sample_ = now;
    }
#endif
    return packet;
  }

  if (q_->byteLength() > 0) {
#if LOG_QUEUE
    if (trace_) {
      double now = Scheduler::instance().clock();
      int qlen = credit_q_->byteLength() + q_->byteLength();
      qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
      last_sample_ = now;
    }
#endif
    return REDQueue::deque();
  }

  if (packet) {
    double delay = (pkt_size - tokens_) / token_refresh_rate_;
    credit_timer_.resched(delay);
  } else if (credit_q_->byteLength() > 0 && q_->byteLength() > 0) {
    fprintf(stderr, "Switch has non-zero queue, but timer was not set.\n");
    exit(1);
  }
#if LOG_QUEUE
  if (trace_) {
    double now = Scheduler::instance().clock();    
    int qlen = credit_q_->byteLength() + q_->byteLength();
    qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
    last_sample_ = now;
  }
#endif
  return NULL;
}

#if LOG_QUEUE

void XPassRED::logQueue() {
  double now = Scheduler::instance().clock();
  int qlen = credit_q_->byteLength() + q_->byteLength();
  FILE *flog;

  qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;

  if (now >= 0.1) {
    char fname[1024];
    sprintf(fname,"outputs/queue_exp%d_%d.tr", exp_id_, qidx_);
    flog = fopen(fname, "a");
    fprintf(flog, "%lf %lf %d\n", now, qavg_, qmax_);
    fclose(flog);
  }

  last_log_ = now;
  last_sample_ = now;

  qavg_ = 0.0;
  qmax_ = 0;
  if (trace_) {
    LogTimer.resched(LOG_GRAN);
  }
}

void EXPQueueLogTimer::expire(Event *) {
  a_->logQueue();
}

#endif
