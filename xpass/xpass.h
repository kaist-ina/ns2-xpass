#ifndef _XPASS_XPASS_H_
#define _XPASS_XPASS_H_

#include "agent.h"
#include "packet.h"
#include "tcp.h"
#include "template.h"
#include <assert.h>

typedef enum XPASS_SEND_STATE_ {
  XPASS_SEND_CLOSED,
  XPASS_SEND_CREDIT_SENDING,
  XPASS_SEND_CREDIT_STOP_RECEIVED,
  XPASS_SEND_NSTATE,
} XPASS_SEND_STATE;

typedef enum XPASS_RECV_STATE_ {
  XPASS_RECV_CLOSED,
  XPASS_RECV_CREDIT_REQUEST_SENT,
  XPASS_RECV_CREDIT_RECEIVING,
  XPASS_RECV_CREDIT_STOP_SENT,
  XPASS_RECV_NSTATE,
} XPASS_RECV_STATE;

struct hdr_xpass {
  // To measure RTT  
  double credit_sent_time_;

  // Credit sequence number
  seq_t credit_seq_;

  // For header access
  static int offset_; // required by PacketHeaderManager
  inline static hdr_xpass* access(const Packet* p) {
    return (hdr_xpass*)p->access(offset_);
  }

  /* per-field member access functions */
  double& credit_sent_time() { return (credit_sent_time_); }
  seq_t& credit_seq() { return (credit_seq_); }
};

class XPassAgent;
class SendCreditTimer: public TimerHandler {
public:
  SendCreditTimer(XPassAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  XPassAgent *a_;
};

class CreditStopTimer: public TimerHandler {
public:
  CreditStopTimer(XPassAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  XPassAgent *a_;
};

class RetransmitTimer: public TimerHandler {
public:
  RetransmitTimer(XPassAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  XPassAgent *a_;
};

class XPassAgent: public Agent {
  friend class SendCreditTimer;
  friend class CreditStopTimer;
  friend class RetransmitTimer;
public:
  XPassAgent(): Agent(PT_XPASS_DATA), credit_send_state_(XPASS_SEND_CLOSED),
                credit_recv_state_(XPASS_RECV_CLOSED), last_credit_rate_update_(-0.0),
                credit_total_(0), credit_dropped_(0), can_increase_w_(false),
                send_credit_timer_(this), credit_stop_timer_(this),
                retransmit_timer_(this), curseq_(1), t_seqno_(1), recv_next_(1),
                c_seqno_(1), c_recv_next_(1), rtt_(-0.0) { }
  virtual int command(int argc, const char*const* argv);
  virtual void recv(Packet*, Handler*);
protected:
  virtual void delay_bind_init_all();
  virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);

  // credit send state
  XPASS_SEND_STATE credit_send_state_;
  // credit receive state
  XPASS_RECV_STATE credit_recv_state_;

  // minimum Ethernet frame size (= size of control packet such as credit)
  int min_ethernet_size_;
  // maximum Ethernet frame size (= maximum data packet size)
  int max_ethernet_size_;

  // If min_credit_size_ and max_credit_size_ are the same, 
  // credit size is determined statically. Otherwise, if
  // min_credit_size_ != max_credit_size_, credit sizes is
  // determined randomly between min and max.
  // minimum credit size (practically, should be > min_ethernet_size_)
  int min_credit_size_;
  // maximum credit size
  int max_credit_size_;

  // maximum credit rate (= lineRate * 84/(1538+84))
  // in Bytes/sec
  int max_credit_rate_;
  // current credit rate (should be initialized ALPHA*max_credit_rate_)
  // should always less than or equal to max_credit_rate_.
  // in Bytes/sec
  int cur_credit_rate_;
  // initial cur_credit_rate_ = alpha_ * max_credit_rate_
  double alpha_;
  // last time for cur_credit_rate_ update with feedback control.
  double last_credit_rate_update_;
  // target loss scaling factor.
  // target loss = (1 - cur_credit_rate/max_credit_rate)*target_loss_scaling.
  double target_loss_scaling_;
  // total number of credit = # credit received + # credit dropped.
  int credit_total_;
  // number of credit dropped.
  int credit_dropped_;
  // aggressiveness factor
  // it determines how aggressively increase the credit sending rate.
  double w_;
  // initial value of w_
  double w_init_;
  // minimum value of w_
  double min_w_;
  // whether feedback control can increase w or not.
  bool can_increase_w_;
  // maximum jitter: -1.0 ~ 1.0 (wrt. inter-credit gap)
  double max_jitter_;
  // minimum jitter: -1.0 ~ 1.0 (wrt. inter-credit gap)
  double min_jitter_;

  SendCreditTimer send_credit_timer_;
  CreditStopTimer credit_stop_timer_;
  RetransmitTimer retransmit_timer_;

  // the highest sequence number produced by app.
  seq_t curseq_;
  // next sequence number to send
  seq_t t_seqno_;
  // next sequence number expected (acknowledging number)
  seq_t recv_next_;
  // next credit sequence number to send
  seq_t c_seqno_;
  // next credit sequence number expected
  seq_t c_recv_next_;

  // weighted-average round trip time
  double rtt_;
  // flow start time
  double fst_;

  // retransmission time out
  double retransmit_timeout_;

  inline double now() { return Scheduler::instance().clock(); }
  seq_t datalen_remaining() { return (curseq_ - t_seqno_); }
  double avg_credit_size() { return (min_credit_size_ + max_credit_size_)/2.0; }

  void init();
  Packet* construct_credit_request();
  Packet* construct_credit_stop();
  Packet* construct_credit();
  Packet* construct_data(Packet *credit);
  void send_credit();
  void send_credit_stop();
  void advance_bytes(seq_t nb);

  void recv_credit_request(Packet *pkt);
  void recv_credit(Packet *pkt);
  void recv_data(Packet *pkt);
  void recv_credit_stop(Packet *pkt);

  void handle_retransmit();
  void process_ack(Packet *pkt);
  void update_rtt(Packet *pkt);

  void credit_feedback_control();
};

#endif
