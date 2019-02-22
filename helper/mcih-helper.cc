/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv6-list-routing.h"
#include "ns3/log.h"

#include "mcih-helper.h"
#include "ns3/mcih.h"

namespace ns3 {
  NS_LOG_COMPONENT_DEFINE( "McihHelper");

  McihHelper::McihHelper(): Ipv6RoutingHelper(), bbvr( 0.0){
    agent_factory.SetTypeId( "ns3::mcih::RoutingProtocol");
  }

  McihHelper::~McihHelper(){
  }

  McihHelper* McihHelper::Copy( void) const{
    return new McihHelper( *this);
  }

  Ptr< Ipv6RoutingProtocol> McihHelper::Create( Ptr< Node> node) const{
    auto agent= agent_factory.Create< mcih::RoutingProtocol>();
    agent->SetDefaultRole( (bbvr>mcih::dist(mcih::engine)?mcih::MasterClusterHead:mcih::Undecided));
    node->AggregateObject( agent);
    return agent;
  }

  void McihHelper::Set( std::string name, const AttributeValue &value){
    agent_factory.Set( name, value );
  }

  int64_t McihHelper::AssignStreams( NodeContainer c, int64_t stream){
    int64_t current_stream= stream;
    Ptr< Node> node;
    for( auto i= c.Begin(); i!= c.End(); ++i ){
      node= ( *i);
      auto ipv6= node->GetObject< Ipv6>();
      NS_ASSERT_MSG( ipv6, "Ipv6 not installed on node");
      auto proto= ipv6->GetRoutingProtocol();
      NS_ASSERT_MSG( proto, "Ipv6 routing not installed on node");
      auto mcih= DynamicCast< mcih::RoutingProtocol>( proto);
      if( mcih){
        current_stream+= mcih->AssignStreams( current_stream);
        continue;
      }
      auto list= DynamicCast< Ipv6ListRouting>( proto);
      if( list){
        int16_t priority;
        Ptr< Ipv6RoutingProtocol> list_proto;
        Ptr< mcih::RoutingProtocol> list_mcih;
        for( uint32_t i= 0; i< list->GetNRoutingProtocols(); i++){
          if( list_mcih){
            current_stream+= list_mcih->AssignStreams( current_stream);
          }
        }
      }
    }
    return ( current_stream- stream);
  }
}

