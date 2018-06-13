#include "tcp-xpass.h"
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#include <assert.h>
static class TcpXPassClass: public TclClass {
public:
  TcpXPassClass(): TclClass("Agent/TCP/FullTcp/XPass") {}
  TclObject* create(int, const char*const*) {
    return (new TcpXPassAgent());
  }
} class_tcp_xpass;

void TcpXPassSendCreditTimer::expire(Event *) {
  a_->send_credit();
}

void TcpXPassCreditStopTimer::expire(Event *) {
  a_->send_credit_stop();
}

void TcpXPassSenderRetransmitTimer::expire(Event *) {
  a_->handle_sender_retransmit();
}

void TcpXPassFCTTimer::expire(Event *) {
  a_->handle_fct();
}

void TcpXPassAgent::delay_bind_init_all() {
  delay_bind_init_one("max_credit_rate_");
  delay_bind_init_one("base_credit_rate_");
  delay_bind_init_one("alpha_");
  delay_bind_init_one("target_loss_scaling_");
  delay_bind_init_one("min_credit_size_");
  delay_bind_init_one("max_credit_size_");
  delay_bind_init_one("min_ethernet_size_");
  delay_bind_init_one("max_ethernet_size_");
  delay_bind_init_one("w_init_");
  delay_bind_init_one("min_w_");
  delay_bind_init_one("retransmit_timeout_");
  delay_bind_init_one("default_credit_stop_timeout_");
  delay_bind_init_one("min_jitter_");
  delay_bind_init_one("max_jitter_");
  delay_bind_init_one("exp_id_");
  delay_bind_init_one("xpass_hdr_size_");
#if CFC_ALGO == CFC_BIC
  delay_bind_init_one("bic_s_min_");
  delay_bind_init_one("bic_s_max_");
  delay_bind_init_one("bic_beta_");
#endif
  FullTcpAgent::delay_bind_init_all();
}

int TcpXPassAgent::delay_bind_dispatch(const char *varName, const char *localName,
                                    TclObject *tracer) {
  if (delay_bind(varName, localName, "max_credit_rate_", &max_credit_rate_,
                tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "base_credit_rate_", &base_credit_rate_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "alpha_", &alpha_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "target_loss_scaling_", &target_loss_scaling_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_credit_size_", &min_credit_size_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_credit_size_", &max_credit_size_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_ethernet_size_", &min_ethernet_size_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_ethernet_size_", &max_ethernet_size_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "w_init_", &w_init_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_w_", &min_w_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "retransmit_timeout_", &retransmit_timeout_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "default_credit_stop_timeout_", &default_credit_stop_timeout_,
                 tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_jitter_", &max_jitter_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_jitter_", &min_jitter_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "exp_id_", &exp_id_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "xpass_hdr_size_", &xpass_hdr_size_, tracer)) {
    return TCL_OK;
  }
#if CFC_ALGO == CFC_BIC
  if (delay_bind(varName, localName, "bic_s_min_", &bic_s_min_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "bic_s_max_", &bic_s_max_, tracer)) {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "bic_beta_", &bic_beta_, tracer)) {
    return TCL_OK;
  }
#endif
  return FullTcpAgent::delay_bind_dispatch(varName, localName, tracer);
}

void TcpXPassAgent::init() {
  w_ = w_init_;
#if CFC_ALG == CFC_BIC
  bic_target_rate_ = max_credit_rate_/2;
#endif
  last_credit_rate_update_ = now();
}

void TcpXPassAgent::recv(Packet* pkt, Handler* h) {
  hdr_cmn *cmnh = hdr_cmn::access(pkt);

  switch (cmnh->ptype()) {
    case PT_XPASS_CREDIT_REQUEST:
      recv_credit_request(pkt);
      Packet::free(pkt);
      break;
    case PT_XPASS_CREDIT:
      recv_credit(pkt);
      Packet::free(pkt);
      break;
    case PT_XPASS_DATA:
      recv_data(pkt);
      FullTcpAgent::recv(pkt, h);
      break;
    case PT_XPASS_CREDIT_STOP:
      recv_credit_stop(pkt);
      Packet::free(pkt);
      break;
    case PT_XPASS_NACK:
      recv_nack(pkt);
      Packet::free(pkt);
      break;
    default:
      FullTcpAgent::recv(pkt, h);
      break;
  }
}

void TcpXPassAgent::recv_credit_request(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);
  double lalpha = alpha_;
  switch (credit_send_state_) {
    case XPASS_SEND_CLOSE_WAIT:
      fct_timer_.force_cancel();
      init();
#if AIR
      if (xph->sendbuffer_ < 40) {
        lalpha = alpha_ * xph->sendbuffer_ / 40.0;
      }
#endif
      cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
      // need to start to send credits.
      send_credit();
        
      // XPASS_SEND_CLOSED -> XPASS_SEND_CREDIT_REQUEST_RECEIVED
      credit_send_state_ = XPASS_SEND_CREDIT_SENDING;     
      break;
    case XPASS_SEND_CLOSED:
      init();
#if AIR
      if (xph->sendbuffer_ < 40) {
        lalpha = alpha_ * xph->sendbuffer_ / 40.0;
      } 
#endif
      cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
      fst_ = xph->credit_sent_time();
      // need to start to send credits.
      send_credit();
        
      // XPASS_SEND_CLOSED -> XPASS_SEND_CREDIT_REQUEST_RECEIVED
      credit_send_state_ = XPASS_SEND_CREDIT_SENDING;     
      break;
  }
  if(cur_credit_rate_ <= 0) {
    fprintf(stderr, "cur_credit_rate_ has been set to negative\n");
    assert(0);
    exit(1);
  }
}

void TcpXPassAgent::recv_credit(Packet *pkt) {
  credit_recved_rtt_++;
  switch (credit_recv_state_) {
    case XPASS_RECV_CREDIT_REQUEST_SENT:
      sender_retransmit_timer_.force_cancel();
      credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
      // first sender RTT.
      rtt_ = now() - rtt_;
      last_credit_recv_update_ = now();
    case XPASS_RECV_CREDIT_RECEIVING:
      // send data
      if (sendbuffer_->length() > 0) {
        send(construct_data(pkt), 0);
      } else {
        credit_wasted_++;
      }
 
      if (datalen_remaining() == 0) {
        if (credit_stop_timer_.status() != TIMER_IDLE) {
          fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
          exit(1);
        }
        // Because ns2 does not allow sending two consecutive packets, 
        // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
        credit_stop_timer_.sched(0);
      }
#if ECS
	 else if (now() - last_credit_recv_update_ >= rtt_) {
        if (credit_recved_rtt_ >= (2 * pkt_remaining())) {
          // Early credit stop
          if (credit_stop_timer_.status() != TIMER_IDLE) {
            fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
            exit(1);
          }
          // Because ns2 does not allow sending two consecutive packets, 
          // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
          credit_stop_timer_.sched(0);
        }
        credit_recved_rtt_ = 0;
        last_credit_recv_update_ = now();
      }
#endif
      break;
    case XPASS_RECV_CREDIT_STOP_SENT:
      if (sendbuffer_->length() > 0) {
        send(construct_data(pkt), 0);
      } else {
        credit_wasted_++;
      }
      credit_recved_++;
      break;
    case XPASS_RECV_CLOSE_WAIT:
      // accumulate credit count to check if credit stop has been delivered
      credit_wasted_++;
      credit_recved_++;
      break;
    case XPASS_RECV_CLOSED:
      credit_wasted_++;
      break;
  }
}

void TcpXPassAgent::recv_data(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);
  // distance between expected sequence number and actual sequence number.
  int distance = xph->credit_seq() - c_recv_next_;

  if (distance < 0) {
    // credit packet reordering or credit sequence number overflow happend.
    fprintf(stderr, "ERROR: Credit Sequence number is reverted.\n");
    exit(1);
  }
  credit_total_ += (distance + 1);
  credit_dropped_ += distance;

  c_recv_next_ = xph->credit_seq() + 1;

  update_rtt(pkt);
}

void TcpXPassAgent::recv_nack(Packet *pkt) {
  fprintf(stderr, "Nack received.");
  exit(1);
}

void TcpXPassAgent::recv_credit_stop(Packet *pkt) {
  fct_ = now() - fst_;
  fct_timer_.resched(default_credit_stop_timeout_);
  send_credit_timer_.force_cancel();
  credit_send_state_ = XPASS_SEND_CLOSE_WAIT;
}

void TcpXPassAgent::handle_fct() {
  char foname[40];
  sprintf(foname, "outputs/fct_%d.out", exp_id_);

  FILE *fct_out = fopen(foname,"a");

  fprintf(fct_out, "%d,%ld,%.10lf\n", fid_, rcv_nxt_-1, fct_);
  fclose(fct_out);
  credit_send_state_ = XPASS_SEND_CLOSED;
}

void TcpXPassAgent::handle_sender_retransmit() {
  switch (credit_recv_state_) {
    case XPASS_RECV_CREDIT_REQUEST_SENT:
      send(construct_credit_request(), 0);
      sender_retransmit_timer_.resched(retransmit_timeout_);
      break;
    case XPASS_RECV_CREDIT_STOP_SENT:
      if (datalen_remaining() > 0) {
        credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
        send(construct_credit_request(), 0);
        sender_retransmit_timer_.resched(retransmit_timeout_);
      } else {
        credit_recv_state_ = XPASS_RECV_CLOSE_WAIT;
        credit_recved_ = 0;
        sender_retransmit_timer_.resched((rtt_ > 0) ? rtt_ : default_credit_stop_timeout_); 
      }
      break;
    case XPASS_RECV_CLOSE_WAIT:
      if (credit_recved_ == 0) {
        char foname[40];
        sprintf(foname, "outputs/waste_%d.out", exp_id_);

        FILE *waste_out = fopen(foname,"a");

        credit_recv_state_ = XPASS_RECV_CLOSED;
        sender_retransmit_timer_.force_cancel();
        fprintf(waste_out, "%d,%ld,%d\n", fid_, curseq_-1, credit_wasted_);
        fclose(waste_out); 
        return;
      }
      // retransmit credit_stop
      send_credit_stop();
      break;
    case XPASS_RECV_CLOSED:
      fprintf(stderr, "Sender Retransmit triggered while connection is closed.");
      exit(1);
  }
}

Packet* TcpXPassAgent::construct_credit_request() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }

  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_REQUEST;

  xph->credit_seq() = 0;
  xph->credit_sent_time_ = now();
#if AIR
  xph->sendbuffer_ = pkt_remaining();
  if(xph->sendbuffer_ < 0) {
    printf("Error: sendbuffer negative\n");
    assert(0);
  }
#endif
  // to measure rtt between credit request and first credit
  // for sender.
  rtt_ = now();

  return p;
}

Packet* TcpXPassAgent::construct_credit_stop() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  
  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_STOP;

  xph->credit_seq() = 0;

  return p;
}

Packet* TcpXPassAgent::construct_credit() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  int credit_size = min_credit_size_;

  if (min_credit_size_ < max_credit_size_) {
    // variable credit size
    credit_size += rand()%(max_credit_size_ - min_credit_size_ + 1);
  } else {
    // static credit size
    if (min_credit_size_ != max_credit_size_){
      fprintf(stderr, "ERROR: min_credit_size_ should be less than or equal to max_credit_size_\n");
      exit(1);
    }
  }

  cmnh->size() = credit_size;
  cmnh->ptype() = PT_XPASS_CREDIT;

  xph->credit_sent_time() = now();
  xph->credit_seq() = c_seqno_;

  c_seqno_ = max(1, c_seqno_+1);

  return p;
}

Packet* TcpXPassAgent::construct_data(Packet *credit) {
  if (sendbuffer_->length() == 0) {
    return NULL;
  }

  Packet *p = sendbuffer_->deque();

  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  hdr_xpass *credit_xph = hdr_xpass::access(credit);

  int datalen = cmnh->size() - tcph->hlen();

  cmnh->ptype() = PT_XPASS_DATA;

  xph->credit_sent_time() = credit_xph->credit_sent_time();
  xph->credit_seq() = credit_xph->credit_seq();
  
  return p;
}

void TcpXPassAgent::send_credit() {
  double avg_credit_size = (min_credit_size_ + max_credit_size_)/2.0;
  double delay;

  credit_feedback_control();

  // send credit.
  send(construct_credit(), 0);

 // calculate delay for next credit transmission.
  delay = avg_credit_size / cur_credit_rate_;
   if(delay<0)
    printf("Negative delay : %lf, avgsize = %lf, curcredi=%d\n", delay, avg_credit_size, cur_credit_rate_);
  // add jitter
  if (max_jitter_ > min_jitter_) {
    double jitter = (double)rand()/(double)RAND_MAX;
    jitter = jitter * (max_jitter_ - min_jitter_) + min_jitter_;
    // jitter is in the range between min_jitter_ and max_jitter_
    delay = delay*(1+jitter);
  }else if (max_jitter_ < min_jitter_) {
    fprintf(stderr, "ERROR: max_jitter_ should be larger than min_jitter_");
    exit(1);
  }
  delay = MAX(delay, 0);
  send_credit_timer_.resched(delay);
}

void TcpXPassAgent::send_credit_stop() {
  send(construct_credit_stop(), 0);
  // set on timer
  sender_retransmit_timer_.resched(rtt_ > 0 ? (2. * rtt_) : default_credit_stop_timeout_);    
  credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT; //Later changes to XPASS_RECV_CLOSE_WAIT -> XPASS_RECV_CLOSED
}


void TcpXPassAgent::update_rtt(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);

  double rtt = now() - xph->credit_sent_time();
  if (rtt_ > 0.0) {
    rtt_ = 0.8*rtt_ + 0.2*rtt;
  }else {
    rtt_ = rtt;
  }
}

void TcpXPassAgent::credit_feedback_control() {
  if (rtt_ <= 0.0) {
    return;
  }
  if ((now() - last_credit_rate_update_) < rtt_) {
    return;
  }
  if (credit_total_ == 0) {
    return;
  }

  int old_rate = cur_credit_rate_;
  double loss_rate = credit_dropped_/(double)credit_total_;
  int min_rate = (int)(avg_credit_size() / rtt_);

#if CFC_ALG == CFC_ORIG
  double target_loss = (1.0 - cur_credit_rate_/(double)max_credit_rate_) * target_loss_scaling_;

  if (loss_rate > target_loss) {
    // congestion has been detected!
    if (loss_rate >= 1.0) {
      cur_credit_rate_ = (int)(avg_credit_size() / rtt_);
    } else {
      cur_credit_rate_ = (int)(avg_credit_size()*(credit_total_ - credit_dropped_)
                         / (now() - last_credit_rate_update_)
                         * (1.0+target_loss));
    }
    if (cur_credit_rate_ > old_rate) {
      cur_credit_rate_ = old_rate;
    }

    w_ = max(w_/2.0, min_w_);
    can_increase_w_ = false;
  }else {
    // there is no congestion.
    if (can_increase_w_) {
      w_ = min(w_ + 0.05, 0.5);
    }else {
      can_increase_w_ = true;
    }

    if (cur_credit_rate_ < max_credit_rate_) {
      cur_credit_rate_ = (int)(w_*max_credit_rate_ + (1-w_)*cur_credit_rate_);
    }
  }
#elif CFC_ALG == CFC_BIC
  double target_loss;
  int data_received_rate;

  if (cur_credit_rate_ >= base_credit_rate_) {
    target_loss = target_loss_scaling;
  } else {
    target_loss = (1.0 - cur_credit_rate_/(double)base_credit_rate_) * target_loss_scaling_;
  }

  if (loss_rate > target_loss) {
    if (loss_rate >= 1.0) {
      data_received_rate = (int)(avg_credit_size() / rtt_);
    } else {
      data_received_rate = (int)(avg_credit_size()*(credit_total_ - credit_dropped_)
          / (now() - last_credit_rate_update_)*(1.0 + target_loss));
    }
    bic_target_rate_ = cur_credit_rate_;
    if (cur_credit_rate_ > data_received_rate)
      cur_credit_rate_ = data_received_rate;

    if (old_rate - cur_credit_rate_ < bic_s_min_) {
      cur_credit_rate_ = old_rate - bic_s_min_;
    } else if (old_rate - cur_credit_rate_ > bic_s_max_) {
      cur_credit_rate_ = old_rate - bic_s_max_;
    }
  } else {
    if (bic_target_rate_ - cur_credit_rate_ <= 0.05*bic_target_rate_) {
      if (cur_credit_rate_ < bic_target_rate_) {
        cur_credit_rate_ = bic_target_rate_;
      } else {
        cur_credit_rate_ = cur_credit_rate_ + (cur_credit_rate_ - bic_target_rate_)
                                             *(1.0 + bic_beta_);
      }
    } else {
      cur_credit_rate_ = (cur_credit_rate_ + bic_target_rate_)/2;
    }
    if (cur_credit_rate_ - old_rate < bic_s_min_) {
      cur_credit_rate_ = old_rate + bic_s_min_;
    } else if (cur_credit_rate_ - old_rate > bic_s_max_) {
      cur_credit_rate_ = old_rate + bic_s_max_;
    }
  }
#endif

  if (cur_credit_rate_ > max_credit_rate_) {
    cur_credit_rate_ = max_credit_rate_;
  }
  if (cur_credit_rate_ < min_rate) {
    cur_credit_rate_ = min_rate;
  }

  credit_total_ = 0;
  credit_dropped_ = 0;
  last_credit_rate_update_ = now();
}

void TcpXPassAgent::sendpacket(seq_t seqno, seq_t ackno, int pflags,
                              int datalen, int reason, Packet *p) {
  if (!p) p = allocpkt();
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_flags *fh = hdr_flags::access(p);
  
  /* build basic header w/options */
  
  tcph->seqno() = seqno;
  tcph->ackno() = ackno;
  tcph->flags() = pflags;
  tcph->reason() |= reason; // make tcph->reason look like ns1 pkt->flags?
  tcph->sa_length() = 0;    // may be increased by build_options()
  tcph->hlen() = tcpip_base_hdr_size_;
  tcph->hlen() += build_options(tcph);
  
  /*
   * Explicit Congestion Notification (ECN) related:
   * Bits in header:
   * 	ECT (EC Capable Transport),
   * 	ECNECHO (ECHO of ECN Notification generated at router),
   * 	CWR (Congestion Window Reduced from RFC 2481)
   * States in TCP:
   *	ecn_: I am supposed to do ECN if my peer does
   *	ect_: I am doing ECN (ecn_ should be T and peer does ECN)
   */
  
  if (datalen > 0 && ecn_ ){
    // set ect on data packets 
    fh->ect() = ect_;	// on after mutual agreement on ECT
  } else if (ecn_ && ecn_syn_ && ecn_syn_next_ && (pflags & TH_SYN) && (pflags & TH_ACK)) {
    // set ect on syn/ack packet, if syn packet was negotiating ECT
    fh->ect() = ect_;
  } else {
    /* Set ect() to 0.  -M. Weigle 1/19/05 */
    fh->ect() = 0;
  }
  
  if (dctcp_)
    fh->ect() = ect_;
  
  if (ecn_ && ect_ && recent_ce_ ) { 
    // This is needed here for the ACK in a SYN, SYN/ACK, ACK
    // sequence.
    pflags |= TH_ECE;
  }
  // fill in CWR and ECE bits which don't actually sit in
  // the tcp_flags but in hdr_flags
  if ( pflags & TH_ECE) {
    fh->ecnecho() = 1;
  } else {
    fh->ecnecho() = 0;
  }
  if ( pflags & TH_CWR ) {
    fh->cong_action() = 1;
  }
  else {
    /* Set cong_action() to 0  -M. Weigle 1/19/05 */
    fh->cong_action() = 0;
  }
  
  /* actual size is data length plus header length */
  
  hdr_cmn *ch = hdr_cmn::access(p);
  ch->size() = datalen + tcph->hlen();
  
  if (datalen <= 0)
    ++nackpack_;
  else {
    ++ndatapack_;
    ndatabytes_ += datalen;
    last_send_time_ = now();	// time of last data
  }
  if (reason == REASON_TIMEOUT || reason == REASON_DUPACK || reason == REASON_SACK) {
    ++nrexmitpack_;
    nrexmitbytes_ += datalen;
  }
  
  last_ack_sent_ = ackno;
  advance_packet(p);
  
  return;
}

void TcpXPassAgent::advance_packet(Packet *p) {
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_tcp *tcph = hdr_tcp::access(p);

  int syn = (tcph->flags() & TH_SYN)?1:0;
  int fin = (tcph->flags() & TH_FIN)?1:0;
  int datalen = cmnh->size() - tcph->hlen();

  if (datalen == 0) {
    if (!fin || (sendbuffer_->length() == 0)) {
      send(p, 0);
      return;
    }else {
      Packet *p = sendbuffer_->tail();
      hdr_tcp *tcph = hdr_tcp::access(p);
      tcph->flags() |= TH_FIN;
      return;
    }
  }

  sendbuffer_->enque(p);

  if (credit_recv_state_ == XPASS_RECV_CLOSED) {
    // send credit request
    send(construct_credit_request(), 0);
    sender_retransmit_timer_.sched(retransmit_timeout_);

    // XPASS_RECV_CLOSED -> XPASS_RECV_CREDIT_REQUEST_SENT
    credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
  }
}
