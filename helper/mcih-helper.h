/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MCIH_HELPER_H
#define MCIH_HELPER_H

// c++ headers
#include <random>

// ns3 standart headers
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv6-routing-helper.h"

// proposal protocol's headers
#include "ns3/mcih.h"

namespace ns3 {
  class McihHelper: public Ipv6RoutingHelper{
    public:
      McihHelper();
      virtual ~McihHelper();
      McihHelper* Copy( void) const;
      virtual Ptr< Ipv6RoutingProtocol> Create( Ptr< Node> node) const;
      void Set( std::string name, const AttributeValue &value);
      int64_t AssignStreams( NodeContainer c, int64_t stream);
      void SetBackboneVehicleRatio( double b){ bbvr= b;}
      double GetBackboneVehicleRatio(){ return bbvr;}
    private:
      ObjectFactory agent_factory;
      double bbvr;  // backbone vehicle ratio
  };
}

#endif /* MCIH_HELPER_H */

