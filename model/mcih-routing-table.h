#ifndef __MCIH_ROUTING_TABLE_H_
#define __MCIH_ROUTING_TABLE_H_

#include <map>

#include "ns3/object.h"
#include "ns3/ipv6.h"
#include "ns3/log.h"
#include "ns3/ipv6-route.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/node.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ptr.h"
#include "ns3/event-id.h"

#include "mcih-utility.h"

namespace ns3{
  namespace mcih{
    enum RouteFlags{ VALID, INVALID, IN_SEARCH };

    class McihRoutingTableEntry;
    class McihRoutingTable;

    class McihRoutingTableEntry: public Ipv6RoutingTableEntry, public Object{
      public:
        enum EntryStatus{
          McihValid, McihInvalid
        };
        McihRoutingTableEntry();
        McihRoutingTableEntry( Ipv6Address network, Ipv6Prefix prefix, Ipv6Address next_hop, uint32_t if_index, Ipv6Address prefix_to_use);
        McihRoutingTableEntry( Ipv6Address network, Ipv6Prefix prefix, uint32_t if_index);
        virtual ~McihRoutingTableEntry(){
        }

        void SetRouteTag( uint16_t route_tag){
          if( tag!= route_tag){
            tag= route_tag;
            changed= true;
          }
        }
        uint16_t GetRouteTag() const{ return tag;}
        void SetRouteMetric( uint8_t route_prefix){
          if( prefix!= route_prefix){
            prefix= route_prefix;
            changed= true;
          }
        }
        uint8_t GetRouteMetric() const{ return prefix;}
        void SetRouteStatus( EntryStatus route_status){
          if( status!= route_status){
            status= route_status;
            changed= true;
          }
        }
        EntryStatus GetRouteStatus() const{ return status;}
        void SetRouteChanged( bool changed){
          this->changed= changed;
        }
        bool IsRouteChanged() const{ return changed;}

      private:
        uint16_t tag;
        uint8_t prefix;
        EntryStatus status;
        bool changed;
    };

    class McihRoutingTable{
      public:
        McihRoutingTable();
        Ptr< Ipv6Route> Lookup( Ipv6Address destination, Ptr< NetDevice> device= 0);
        Ptr< McihRoutingTableEntry> entry;
        void SetIpv6( Ptr< Ipv6> ipv6){ this->ipv6= ipv6;}
        void SetGateway( Ipv6Address gateway){ this->gateway= gateway;}
        Ipv6Address GetGateway(){ return gateway;}
        void AddRoute( Ptr< McihRoutingTableEntry> entry, EventId event){ routes.insert( std::make_pair( entry, event));}
      private:
        std::map< Ptr< McihRoutingTableEntry>, EventId> routes;
        Ptr< Ipv6> ipv6;
        Ipv6Address gateway;
    };
  }
}
#endif // __MCIH_ROUTING_TABLE_H_
