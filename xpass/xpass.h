#ifndef _XPASS_XPASS_H_
#define _XPASS_XPASS_H_

#include "agent.h"
#include "packet.h"

typedef enum xpass_type_ {
  XPASS_CREDIT_REQUEST,
  XPASS_CREDIT_STOP,
  XPASS_CREDIT,
  XPASS_DATA
} XPassPktType;

struct hdr_xpass {
  // Packet type
  XPassPktType pkt_type;
  
  // To measure RTT  
  double credit_sent_time;

  // Credit sequence number
  long long int sequence_number;

  // For header access
  static int offset_; // required by PacketHeaderManager
  inline static hdr_xpass* access(const Packet* p) {
    return (hdr_xpass*)p->access(offset_);
  }
};

class XPassAgent: public Agent {
public:
  XPassAgent();
  virtual int command(int argc, const char*const* argv);
  virtual void recv(Packet*, Handler*);
};

#endif
