#include "xpass.h"

int hdr_xpass::offset_;
static class XPassHeaderClass: public PacketHeaderClass {
public:
  XPassHeaderClass(): PacketHeaderClass("PacketHeader/XPass", sizeof(hdr_xpass)) {
    bind_offset (&hdr_xpass::offset_);
  }
} class_xpass_hdr;

static class XPassClass: public TclClass {
public:
  XPassClass(): TclClass("Agent/XPass") {}
  TclObject* create(int, const char*const*) {
    return (new XPassAgent());
  }
} class_xpass;

XPassAgent::XPassAgent(): Agent(PT_XPASS_CREDIT) {

}

int XPassAgent::command(int argc, const char*const* argv) {

  return Agent::command(argc, argv);
}

void XPassAgent::recv (Packet* pkt, Handler*) {

}
