/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1997 by the University of Southern California
 * $Id: classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $ (USC/ISI)";
#endif

#include "classifier.h"
#include "ip.h"

static int slotcmp (const void *a, const void *b) {
  return (int)(*(unsigned int *)a - *(unsigned int *)b);
}

class MultiPathForwarder : public Classifier {
public:
  MultiPathForwarder() : ns_(0), nodetype_(0), symmetric_(0), sorted_maxslot_(-1) {
    bind("nodetype_", &nodetype_);
    bind_bool("symmetric_", &symmetric_);
  } 
	virtual int classify(Packet* p) {
		int cl;
    if (symmetric_) {
      // If there exists at least one slot and
      // not yet sorted
      if (sorted_maxslot_ == -1 || maxslot_ > sorted_maxslot_) {
        qsort (slot_, maxslot_+1, sizeof(NsObject*), slotcmp);
        sorted_maxslot_ = maxslot_;
      }
      hdr_ip* iph = hdr_ip::access(p);

      struct hkey {
        int fid;
        int nodetype;
        nsaddr_t lower_addr, higher_addr;
      };
      struct hkey buf_;
      nsaddr_t src = mshift(iph->saddr());
      nsaddr_t dst = mshift(iph->daddr());
      int* bufInteger;
      int bufLength;
      unsigned int ms_;

      buf_.nodetype = nodetype_;
      buf_.lower_addr = (src < dst) ? src : dst;
      buf_.higher_addr = (src > dst) ? src : dst;
      buf_.fid = iph->flowid();

      bufInteger = (int*) &buf_;
      bufLength = sizeof(hkey)/sizeof(int);

      ms_ = (unsigned int)HashString(bufInteger, bufLength);
      ms_ %= (maxslot_ + 1);
      unsigned int fail = ms_;
      do {
        cl = ms_++;
        ms_ %= (maxslot_ + 1);
      } while (slot_[cl] == 0 && ms_ != fail);
    } else {
		  int fail = ns_;
		  do {
		  	cl = ns_++;
		  	ns_ %= (maxslot_ + 1);
		  } while (slot_[cl] == 0 && ns_ != fail);
    }
    return cl;
	}
private:
	int ns_;
  // Symmetric Routing
  // "True" for symmetric routing,
  // "False" for asymmetric routing (default)
  int symmetric_;

  int nodetype_;
  int sorted_maxslot_;

  static unsigned int
  HashString(register const int *ints, int length) {
    register unsigned int result;
    register int i;

    result = 0;
    for (i = 0; i < length; i++) {
      srand(*ints++);
      int ran = rand();
      result = result ^ ran;
    }
    srand(result);
    result = rand();
    return result;
  }
};

static class MultiPathClass : public TclClass {
public:
	MultiPathClass() : TclClass("Classifier/MultiPath") {} 
	TclObject* create(int, const char*const*) {
		return (new MultiPathForwarder());
	}
} class_multipath;
