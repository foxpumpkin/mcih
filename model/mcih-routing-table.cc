#include "mcih-routing-table.h"
#include "ns3/log.h"
#include <iomanip>

using namespace std;

namespace ns3{
  NS_LOG_COMPONENT_DEFINE ("McihRoutingTable");
  namespace mcih{
    McihRoutingTableEntry::McihRoutingTableEntry(): tag( 0), prefix( 16), status( McihInvalid), changed( false){
    }
    
    McihRoutingTableEntry::McihRoutingTableEntry( Ipv6Address network, Ipv6Prefix prefix, Ipv6Address next_hop, uint32_t if_index, Ipv6Address prefix_to_use): // sub class constructo
      Ipv6RoutingTableEntry( McihRoutingTableEntry::CreateNetworkRouteTo( network, prefix, next_hop, if_index, prefix_to_use)), // super class constructor
      tag( 0), prefix( 16), status( McihInvalid), changed( false){ // initializer
    }

    McihRoutingTableEntry::McihRoutingTableEntry( Ipv6Address network, Ipv6Prefix prefix, uint32_t if_index): 
      Ipv6RoutingTableEntry( Ipv6RoutingTableEntry::CreateNetworkRouteTo( network, prefix, if_index)), 
      tag( 0), prefix( 16), status( McihInvalid), changed( false){
    }

    McihRoutingTable::McihRoutingTable(){
      NS_LOG_FUNCTION( Utility::Coloring( YELLOW, "Mcih Routing Table"));
    }

    Ptr< Ipv6Route> McihRoutingTable::Lookup( Ipv6Address destination, Ptr< NetDevice> device){
      NS_LOG_FUNCTION( this<< destination<< device);
      if( !ipv6) throw invalid_argument( "mcih routing table is need to set ipv6");

      Ptr< Ipv6Route> route_entry= 0;
      uint16_t longest_mask= 0;

      if( destination.IsLinkLocalMulticast()){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "routing entry for link local multiacst"));
        NS_ASSERT_MSG( device, "no device");
        route_entry= Create< Ipv6Route>();
        route_entry->SetSource( ipv6->SourceAddressSelection( ipv6->GetInterfaceForDevice( device), destination));
        route_entry->SetGateway( Ipv6Address::GetZero());
        route_entry->SetOutputDevice( device);
        route_entry->SetDestination( destination);
        return route_entry;
      }

      if( destination.IsLinkLocal()){
      // NS_ASSERT_MSG( device, "no device");
      //if( !device) device= ipv6->GetNetDevice( 1); 
      device= ipv6->GetNetDevice( 1); 

        NS_LOG_LOGIC( Utility::Coloring( CYAN, "routing entry for link local"));
        route_entry= Create< Ipv6Route>();
        route_entry->SetSource( ipv6->SourceAddressSelection( ipv6->GetInterfaceForDevice( device), destination));
        // route_entry->SetGateway( Ipv6Address::GetAllRoutersMulticast());
        route_entry->SetGateway( destination);
        // route_entry->SetGateway( Ipv6Address::GetZero());
        route_entry->SetOutputDevice( device);
        route_entry->SetDestination( destination);
        return route_entry;
      }

      for( auto itr= routes.begin(); itr!= routes.end(); itr++){
        auto entry= itr->first;
        if( entry->GetRouteStatus()== McihRoutingTableEntry::McihValid){
          auto prefix= entry->GetDestNetworkPrefix();
          auto mask_length= prefix.GetPrefixLength();
          auto address= entry->GetDestNetwork();

          NS_LOG_LOGIC( string( Utility::Coloring( CYAN, "destination"))<< ": "<< destination<< ", "<< string( Utility::Coloring( CYAN, "mask length"))<< ": "<< mask_length);

          if( prefix.IsMatch( destination, address)){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "found global network route ")<< entry);
            // NS_LOG_LOGIC( entry->GetInterface());
            if( !device|| device== ipv6->GetNetDevice( entry->GetInterface())){
            // NS_LOG_LOGIC( Utility::Coloring( RED, "TEST"));
              if( mask_length< longest_mask){
                NS_LOG_LOGIC( Utility::Coloring( CYAN, "previous match longer, skpping"));
                continue;
              }
              longest_mask= mask_length;

              auto if_index= entry->GetInterface();
              route_entry= Create< Ipv6Route>();

              if( entry->GetGateway().IsAny()){
                route_entry->SetSource( ipv6->SourceAddressSelection( if_index, entry->GetDest()));
              } else if( entry->GetDest().IsAny()){ // default route
                route_entry->SetSource( ipv6->SourceAddressSelection( if_index, entry->GetPrefixToUse().IsAny()? destination: entry->GetPrefixToUse()));
              } else{
                route_entry->SetSource( ipv6->SourceAddressSelection( if_index, entry->GetDest()));
              }

              route_entry->SetDestination( entry->GetDest());
              route_entry->SetGateway( entry->GetGateway());
              route_entry->SetOutputDevice( ipv6->GetNetDevice( if_index));
            }
          }
        }
      }
      if( route_entry){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "matching route via")<< route_entry->GetDestination());
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "through")<< route_entry->GetGateway());
      }
      return route_entry;
    }
  }
}
