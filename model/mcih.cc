/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag);

#define NS_LOG_APPEND_CONTEXT if( ipv6){ char msg[ 255]; sprintf( msg, "[node%d/%.3lfs]", ipv6->GetObject< Node>()->GetId(), Simulator::Now().GetMilliSeconds()/1000.0); std::clog<< msg;} 

#include <exception>
#include <algorithm>
#include <limits>

#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/ipv6-interface.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/ipv6-packet-info-tag.h"
#include "ns3/abort.h"
#include "ns3/loopback-net-device.h"
#include "ns3/udp-header.h"

#include "mcih.h"
#include "mcih-utility.h"

using namespace std;

namespace ns3{
  NS_LOG_COMPONENT_DEFINE( "McihRoutingProtocol");
  namespace mcih{
    NS_OBJECT_ENSURE_REGISTERED( RoutingProtocol);
    const uint32_t RoutingProtocol::MCIH_PORT= 1701;

    RoutingProtocol::RoutingProtocol():
      ipv6(),
      node(),
      loopback_device(),
      role( Undecided),
      hello_interval( Seconds( 1)),
      active_route_timeout( MilliSeconds( 5000)),
      active_neighbor_timeout( MilliSeconds( 5000)),
      active_member_timeout( MilliSeconds( 5000)),
      role_check_interval( MilliSeconds( 500)), 
      velocity_check_interval( MilliSeconds( 100)),
      elect_mch_interval( role_check_interval* 2),
      contention_interval( role_check_interval* 4),
      role_check_timer( Timer::CANCEL_ON_DESTROY),
      velocity_check_timer( Timer::CANCEL_ON_DESTROY),
      elect_mch_timer( Timer::CANCEL_ON_DESTROY),
      mcih_routing_table(),
      neighbor_nodes( hello_interval),
      neighbor_headers( hello_interval),
      cluster_members(),
      interface_exclusions(),
      uniform_random_variable( new UniformRandomVariable),
      position( 0, 0, 0),
      velocity( 0, 0, 0),
      initialized( false),
      unbound( 1),
      default_role( Undecided){
        if( ipv6) node= ipv6->GetObject< Node>();
        NS_LOG_FUNCTION( Utility::Coloring( CYAN, "mcih construct"));
      }

    RoutingProtocol::~RoutingProtocol(){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::NotifyAddAddress( uint32_t if_index, Ipv6InterfaceAddress address){
      NS_LOG_FUNCTION( "interface"<< Utility::Coloring( CYAN, if_index));
      NS_LOG_LOGIC( string( Utility::Coloring( CYAN, Utility::InterfaceAddress( address))));

      auto l3= ipv6->GetObject< Ipv6L3Protocol>();
      if( l3->IsUp( if_index)){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is up"));
      } else{
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is not up"));
        return;
      }

      if( interface_exclusions.find( if_index)== interface_exclusions.end()){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is live: ")<< if_index);
      } else{
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is exclusion: ")<< if_index);
        return;
      }

      auto device= l3->GetNetDevice( if_index);
      auto interface= l3->GetInterface( if_index);
      auto network_address= address.GetAddress().CombinePrefix( address.GetPrefix());
      auto network_prefix= address.GetPrefix();

      if( address.GetAddress()!= Ipv6Address()&& address.GetPrefix()!= Ipv6Prefix()){
        // AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag);
        // routing_table.AddRoute( device, network_address,);
        AddNetworkRouteTo( network_address, network_prefix, if_index);
      }

      SendTriggeredRouteUpdate();
    }

    void RoutingProtocol::NotifyAddRoute( Ipv6Address dst, Ipv6Prefix mask, Ipv6Address next_hop, uint32_t if_num, Ipv6Address prefix_to_use){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< dst);
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "prefix: ")<< mask);
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "next hop ")<< next_hop);
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "interface index: ")<< if_num);
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "prefix to use: ")<< prefix_to_use);
    }

    void RoutingProtocol::NotifyInterfaceDown( uint32_t if_num){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::NotifyInterfaceUp( uint32_t if_num){
      NS_LOG_FUNCTION( this<< if_num);

      // adding route(ripng)
      for( uint32_t ad_index= 0; ad_index< ipv6->GetNAddresses( if_num); ad_index++){
        auto address= ipv6->GetAddress( if_num, ad_index);
        auto network_mask= address.GetPrefix();
        auto network_address= address.GetAddress().CombinePrefix( network_mask);
        if( address!= Ipv6Address()&& network_mask!= Ipv6Prefix()){
          AddNetworkRouteTo( network_address, network_mask, if_num);
        }
      }

      auto l3= ipv6->GetObject< Ipv6L3Protocol>();
      auto interface= l3->GetInterface( if_num);
      interface->SetForwarding( false);
      auto device= interface->GetDevice();
      // auto device= l3->GetNetDevice( if_num);
      auto ndisc= interface->GetNdiscCache();
      if( ndisc){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "interface has ndisc cache: ")<< ndisc);
        neighbor_nodes.AddNdiscCache( ndisc);
      }

      if( !initialized){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "not initializing yet"));
        return;
      }

      auto wifi= device->GetObject< WifiNetDevice>();
      if( wifi){
        auto mac= wifi->GetMac();
        if( mac){
          mac->TraceConnectWithoutContext( "TxErrHeader", neighbor_nodes.GetTxErrorCallback());
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "set tx error callback"));
        } else{
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "could not get mac address from wifi object"));
          return;
        }
      } else{
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "could not get wifi object"));
      }

      bool send_socket_found= FindSocket( interface);
      if( send_socket_found){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "socket is found"));
      } else{
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "socket is not found"));
      }

      bool active_interface= interface_exclusions.find( ipv6->GetInterfaceForDevice( device))!= interface_exclusions.end();
      if( send_socket_found){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is active"));
      } else{
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is not active"));
      }

      for( int ad_index= 0; ad_index< interface->GetNAddresses(); ad_index++){
        auto address= interface->GetAddress( ad_index);
        if( address.GetScope()== Ipv6InterfaceAddress::LINKLOCAL&& !send_socket_found&& active_interface){
          NS_LOG_LOGIC ("RIPng: adding sending socket to " << address.GetAddress ());
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
          Ptr<Node> theNode = GetObject<Node> ();
          Ptr<Socket> socket = Socket::CreateSocket (theNode, tid);
          Inet6SocketAddress local = Inet6SocketAddress (address.GetAddress (), MCIH_PORT);
          socket->Bind (local);
          socket->BindToNetDevice( device);
          socket->ShutdownRecv();
          socket->SetIpv6RecvHopLimit( true);
          socket_interfaces.insert( make_pair( socket, interface));
        } else if( address.GetScope()== Ipv6InterfaceAddress::GLOBAL){
          SendTriggeredRouteUpdate ();
        }
      }
    }

    void RoutingProtocol::NotifyRemoveAddress( uint32_t if_num, Ipv6InterfaceAddress address){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::NotifyRemoveRoute (Ipv6Address destination, Ipv6Prefix mask, Ipv6Address next_hop, uint32_t if_num, Ipv6Address prefixToUse){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::PrintRoutingTable( Ptr< OutputStreamWrapper> stream) const{
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    Ptr< Ipv6Route> RoutingProtocol::RouteOutput( Ptr< Packet> packet, const Ipv6Header &header, Ptr< NetDevice> output_interface, Socket::SocketErrno &sockerr){
      //NS_LOG_FUNCTION( this);
      if( !packet){
        NS_LOG_FUNCTION( Utility::Coloring( RED, "PACKET IS NULL"));
      }
      NS_LOG_DEBUG( Utility::Coloring( CYAN, header)<< " - "<< Utility::Coloring( CYAN, "interface: ")<< ( int)(output_interface? ipv6->GetInterfaceForDevice( output_interface): -1));

      //auto udp= packet->GetObject< UdpHeader>();

      auto destination= header.GetDestinationAddress();
      Ptr< Ipv6Route> route_entry= 0;
      if( destination.IsLinkLocal()){
        if( destination.IsMulticast()){
          NS_LOG_INFO( Utility::Coloring( CYAN, "destination is link local multicast"));
        } else{
          NS_LOG_INFO( Utility::Coloring( CYAN, "destination is link local unicast"));
        }
      }

      NS_LOG_INFO( Utility::Coloring( CYAN, "number of interface: ")<< ipv6->GetNInterfaces());
      for( int if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        // Print( LOG_LOGIC, GREEN, l3->GetInterface( if_index));
      }

      route_entry= mcih_routing_table.Lookup( destination, output_interface);
      if( route_entry){
        NS_LOG_INFO( Utility::Coloring( CYAN, "route entry found")<< " - "<<  destination);
        Print( LOG_DEBUG, GREEN, route_entry);
        // NS_LOG_LOGIC( "if index "<< ipv6->GetInterfaceForDevice( route_entry->GetOutputDevice()));
        sockerr= Socket::ERROR_NOTERROR;
      } else{
        NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "route entry not found")<< " - "<< destination);
        sockerr= Socket::ERROR_NOROUTETOHOST;
      }
      return route_entry;
    }

    bool RoutingProtocol::RouteInput( Ptr< const Packet> packet, const Ipv6Header &header, Ptr< const NetDevice> device, UnicastForwardCallback unicast_callback, MulticastForwardCallback multicast_callback, LocalDeliverCallback local_callback, ErrorCallback error_callback){
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, packet->GetUid()));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< header.GetDestinationAddress()
          << ", "<< "device: "<< device->GetAddress());

      if( receive_socket_interfaces.empty()){
        NS_LOG_ERROR( Utility::Coloring( RED, "no interface" ));
        return false;
      }

      NS_ASSERT( ipv6!= 0);
      NS_ASSERT( packet!= 0);
      NS_ASSERT( ipv6->GetInterfaceForDevice( device)>= 0);
      int32_t if_index_for_device= ipv6->GetInterfaceForDevice( device);

      auto destination= header.GetDestinationAddress();
      auto source= header.GetSourceAddress();

      if( destination.IsMulticast()){
        NS_LOG_LOGIC( Utility::Coloring( RED, "route input detects multicast address, but not implement yet"));
      }

      for( uint32_t if_index = 0; if_index< ipv6->GetNInterfaces(); if_index++){
        for (uint32_t ad_index= 0; ad_index< ipv6->GetNAddresses( if_index); ad_index++){
          auto address = ipv6->GetAddress( if_index, ad_index);
          //Ipv6Address addr = iaddr.GetAddress ();
          if( address.GetAddress().IsEqual( header.GetDestinationAddress())){
            if( if_index== if_index_for_device){
              NS_LOG_LOGIC ("For me (destination " << address << " match)");
            } else{
              NS_LOG_LOGIC ("For me (destination " << address << " match) on another interface " << header.GetDestinationAddress ());
            }
            local_callback( packet, header, if_index_for_device);
            return true;
          }
          NS_LOG_LOGIC ("Address " << address << " not a match");
        }
      }

      if( header.GetDestinationAddress().IsLinkLocal()|| header.GetSourceAddress().IsLinkLocal()){
        NS_LOG_LOGIC( "Dropping packet not for me and with src or dst LinkLocal");
        error_callback( packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
      }

      if( ipv6->IsForwarding( if_index_for_device)== false){
        NS_LOG_LOGIC( "Forwarding disabled for this interface");
        error_callback( packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
      }

      NS_LOG_LOGIC ("Unicast destination");
      Ptr<Ipv6Route> rtentry = mcih_routing_table.Lookup( header.GetDestinationAddress());

      if( rtentry!= 0){
        NS_LOG_LOGIC ("Found unicast destination - calling unicast callback");
        unicast_callback( device, rtentry, packet, header);
        return true;
      } else{
        NS_LOG_LOGIC ("Did not find unicast destination - returning false");
        return false;
      }

    }

    void RoutingProtocol::SetIpv6( Ptr< Ipv6> arg_ipv6){
      NS_LOG_FUNCTION( this<< arg_ipv6);
      if( !arg_ipv6) throw invalid_argument( "arg ipv6 is null.");
      if( ipv6) throw invalid_argument( "ipv6 is not null.");
      ipv6= arg_ipv6;

      for( int if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        auto interface= l3->GetInterface( if_index);
        if( interface->IsUp()){
          NotifyInterfaceUp( if_index);
        } else{
          NotifyInterfaceDown( if_index);
        }
      }

      for( int if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        auto interface= l3->GetInterface( if_index);
        if( interface->IsUp()){
          NotifyInterfaceUp( if_index);
        } else{
          NotifyInterfaceDown( if_index);
        }
      }
      Simulator::ScheduleNow( &RoutingProtocol::Start, this);
    }

    TypeId RoutingProtocol::GetTypeId(){
      // NS_LOG_FUNCTION( Utility::Coloring( RED, "get type id"));
      static TypeId tid= TypeId( "ns3::mcih::RoutingProtocol")
        .SetParent< Ipv6RoutingProtocol>()
        .SetGroupName( "Mcih")
        .AddConstructor< RoutingProtocol>()
        ;   
      return tid;
    }

    int64_t RoutingProtocol::AssignStreams( int64_t stream){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      uniform_random_variable->SetStream( stream);
      return 1;
    }

    void RoutingProtocol::SendHello( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      if( !destination.IsLinkLocalMulticast())
        throw invalid_argument( "hello is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_HELLO);
      HelloHeader hello;
      hello.SetAddress( GetAddress( 1, Ipv6InterfaceAddress::GLOBAL));
      hello.SetPosition( position);
      hello.SetVelocity( velocity);
      hello.SetRelativePositionAndMobility( GetRPM());
      hello.SetRelativeStateAndMobility( 0);
      hello.SetRole( role);
      hello.SetAbsCm( cluster_members? cluster_members->GetNeighborNumber(): 0);
      packet->AddHeader( hello);
      packet->AddHeader( type);

      NS_LOG_LOGIC( "ROLE SEND: "<< ToString( role));

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }

      NS_LOG_INFO( Utility::Coloring( CYAN, "sent hello"));
    }

    void RoutingProtocol::SendUnadv( Ipv6Address destination){
      NS_ABORT_MSG( "This function must not use.");

      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "undecided advertisement is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_UNADV);
      UnadvHeader unadv;
      unadv.SetPosition( position);
      unadv.SetVelocity( velocity);
      unadv.SetRelativePositionAndMobility( GetRPM());
      packet->AddHeader( unadv);
      packet->AddHeader( type);

      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendElectMch( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      // Print( LOG_LEVEL_LOGIC, GREEN, neighbor_nodes.GetNdiscCache());
      // if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "destination is only link local multicast for electmch");
      // destination= Ipv6Address::GetAllRoutersMulticast();

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_ELECTMCH);
      ElectMchHeader electmch;
      electmch.SetTargetAddress( neighbor_nodes.GetLowestRpmNeighborAddress());
      packet->AddHeader( electmch);
      packet->AddHeader( type);


      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        Print( LOG_DEBUG, CYAN, interface);
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendMchadv( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "undecided advertisement is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_MCHADV);
      MchadvHeader mchadv;
      mchadv.SetPosition( position);
      mchadv.SetVelocity( velocity);
      mchadv.SetRelativePositionAndMobility( GetRPM());
      mchadv.SetMchAddress( GetAddress( 1, Ipv6InterfaceAddress::GLOBAL));
      packet->AddHeader( mchadv);
      packet->AddHeader( type);

      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendRgstreq( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      // if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "registration request is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_RGSTREQ);
      RgstreqHeader header;
      header.SetTargetAddress( neighbor_headers.GetLowestRpmNeighborAddress());
      header.SetRegistAddress( GetAddress( 1, Ipv6InterfaceAddress::GLOBAL));
      packet->AddHeader( header);
      packet->AddHeader( type);

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendRgstrep( Ipv6Address destination, Ipv6Address target){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      cluster_members->Update( destination, active_member_timeout);
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      // if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "registration request is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_RGSTREP);
      RgstrepHeader header;
      header.SetHeaderAddress( GetAddress( 1, Ipv6InterfaceAddress::GLOBAL));// neighbor_headers.GetLowestRpmNeighborAddress());
      packet->AddHeader( header);
      packet->AddHeader( type);

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendResign( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_DEBUG( Utility::Coloring( CYAN, "destination: ")<< destination);
      // if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "registration request is only link local multicast" );

      auto packet= Create< Packet>();
      SocketIpv6HopLimitTag hoplimit_tag;
      packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 0);
      packet->AddPacketTag( hoplimit_tag);

      TypeHeader type( MCIHTYPE_CHRESIGN);
      ResignHeader header;
      packet->AddHeader( header);
      packet->AddHeader( type);

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "socket interface size ")<< socket_interfaces.size());
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        // Print( LOG_LOGIC, CYAN, interface);
        // socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
        // SendTo( socket, packet, destination);
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
      SetRole( Undecided);
    }

    void RoutingProtocol::SetRole( Role r){
      NS_LOG_FUNCTION( this<< Utility::Coloring( CYAN, ToString( r)));
      if( role== r) return;


      if( role== Undecided){
        become_connectable_time= Simulator::Now();
      }
      if( r== Undecided){
        connectable.push_back( Simulator::Now()- become_connectable_time);
      }

      if( r== Undecided){
        neighbor_headers.SetOwnClusterHead( Ipv6Address::GetAny());
      }

      switch( role){
        case Undecided:
          if( r== ClusterMember){ // from undecided to cluster member
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "undecided -> cluster member"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else if( r== MasterClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "undecided -> master cluster head"));
            if( !cluster_members)
              cluster_members= unique_ptr< ClusterMembers>( new ClusterMembers( hello_interval));
            EmptyCheckUpdate( contention_interval);
          } else throw invalid_argument( "invalid updating role to without cluster member from undecided");
          break;
          
        case ClusterMember:
          if( r== Undecided){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> undecided"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
            ElectMchUpdate(elect_mch_interval);
          } else if( r==SubClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> sub cluster head"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else if( r==MasterClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> master cluster head"));
            if( !cluster_members)
              cluster_members= unique_ptr< ClusterMembers>( new ClusterMembers( hello_interval));
            EmptyCheckUpdate( contention_interval);
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else throw invalid_argument( "invalid role");
          break;

        case SubClusterHead:
          if( r== Undecided){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> undecided"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
            ElectMchUpdate(elect_mch_interval);
          } else if( r== ClusterMember){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> cluster member"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else if( r== MasterClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> master cluster head"));
            if( !cluster_members)
              cluster_members= unique_ptr< ClusterMembers>( new ClusterMembers( hello_interval));
            EmptyCheckUpdate( contention_interval);
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else throw invalid_argument( "invalid role");
          break;

        case MasterClusterHead:
          delete cluster_members.release(); // purgins member list
          if( r== Undecided){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "master cluster head -> undecided"));
            ElectMchUpdate(elect_mch_interval);
          } else if( r== ClusterMember){
          } else throw invalid_argument( "invalid role");
          break;
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          NS_LOG_LOGIC( Utility::Coloring( RED, "master cluster head, but not implement yet"));
          break;
        case RoleNumber:
          throw invalid_argument( "invalid role");
          break;
      }
      role= r;
    }

    void RoutingProtocol::SetDefaultRole( Role r){
      NS_LOG_LOGIC( this<< " set default role: "<< ToString(r));
      default_role= r;
    }

    void RoutingProtocol::Start(){
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "node launch"));
      if( !ipv6) throw invalid_argument( "need ipv6 pointer");
      mcih_routing_table.SetIpv6( ipv6);
      role_check_timer.SetFunction( &RoutingProtocol::RoleCheckTimerExpire, this);
      role_check_timer.Schedule( role_check_interval);
      velocity_check_timer.SetFunction( &RoutingProtocol::VelocityCheckTimerExpire, this);
      velocity_check_timer.Schedule( velocity_check_interval);
      elect_mch_timer.SetFunction( &RoutingProtocol::ElectMchTimerExpire, this);
      ElectMchUpdate(elect_mch_interval);
      empty_check_timer.SetFunction( &RoutingProtocol::EmptyCheckTimerExpire, this);
      // role= default_role;
      // if( role== MasterClusterHead){ EmptyCheckUpdate( contention_interval); }
    }

    Ptr< Socket> RoutingProtocol::FindReceiveSocket( Ptr< Ipv6Interface> interface) const{
      NS_LOG_FUNCTION( this);
      Print( LOG_DEBUG, CYAN, interface);
      for( auto itr= receive_socket_interfaces.begin(); itr!= receive_socket_interfaces.end(); itr++){
        auto socket= itr->first;
        auto itr_if= itr->second;
        if( interface== itr_if) return socket;
      }
      return Ptr< Socket>();
    }

    Ptr< Socket> RoutingProtocol::FindSocket( Ptr< Ipv6Interface> interface) const{
      NS_LOG_FUNCTION( this);
      Print( LOG_DEBUG, CYAN, interface);
      for( auto itr= socket_interfaces.begin(); itr!= socket_interfaces.end(); itr++){
        auto socket= itr->first;
        auto itr_if= itr->second;
        if( interface== itr_if) return socket;
      }
      return Ptr< Socket>();
    }

    Ptr< Ipv6Interface> RoutingProtocol::FindInterface( Ptr< Socket> socket) const{
      NS_LOG_FUNCTION( this);//Utility::Coloring( CYAN, Utility::InterfaceAddress( address)));
      for( auto itr= socket_interfaces.begin(); itr!= socket_interfaces.end(); itr++){
        auto itr_socket= itr->first;
        auto itr_interface= itr->second;
        if( itr_socket== socket) return itr_interface;
      }
      return Ptr< Ipv6Interface>();
    }

    Ptr< Ipv6Interface> RoutingProtocol::FindReceiveInterface( Ptr< Socket> socket) const{
      NS_LOG_FUNCTION( this);//Utility::Coloring( CYAN, Utility::InterfaceAddress( address)));
      for( auto itr= receive_socket_interfaces.begin(); itr!= receive_socket_interfaces.end(); itr++){
        auto itr_socket= itr->first;
        auto itr_interface= itr->second;
        if( itr_socket== socket) return itr_interface;
      }
      return Ptr< Ipv6Interface>();
    }

    void RoutingProtocol::ReceiveCallback( Ptr< Socket> socket){
      NS_LOG_FUNCTION( this);//Utility::Coloring( RED, "not implement yet"));
      auto packet= socket->Recv();
      NS_LOG_INFO( Utility::Coloring( CYAN, "Received ")<< *packet);

      // Ipv6PacketInfoTag packet_info;
      // if( !packet->RemovePacketTag( packet_info)){
      // NS_ABORT_MSG( "No incoming interface on RIPng message, aborting.");
      // }
      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "removed packet information tag"));
      // NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "comment out for debug"));

      auto interface= FindReceiveInterface( socket);
      if( !interface){
        throw invalid_argument( "unknown interface is set in receive callback");
      }
      auto device= interface->GetDevice();

      SocketIpv6HopLimitTag hoplimit_tag;
      if( !packet->RemovePacketTag( hoplimit_tag)){
        NS_ABORT_MSG ("No incoming Hop Count on RIPng message, aborting.");
      }
      uint8_t hoplimit= hoplimit_tag.GetHopLimit();
      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "removed hoplimit tag"));

      SocketAddressTag tag;
      if( !packet->RemovePacketTag( tag)){
        NS_ABORT_MSG( "sender address can not detection, aborting");
      }
      auto addr= Inet6SocketAddress::ConvertFrom( tag.GetAddress ());
      NS_LOG_LOGIC( string( Utility::Coloring( CYAN, "received one packet from "))<< addr.GetIpv6()<< ", "
          << string( Utility::Coloring( CYAN, "own ip address "))<< interface->GetLinkLocalAddress().GetAddress());
      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "removed packet tag"));

      auto sender_address= Inet6SocketAddress::ConvertFrom( tag.GetAddress()).GetIpv6();
      uint16_t sender_port= Inet6SocketAddress::ConvertFrom( tag.GetAddress()).GetPort();
      int32_t interface_for_address = ipv6->GetInterfaceForAddress( sender_address);
      if( interface_for_address!= -1){
        NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "ignoring a packet sent by myself"));
        return;
      }

      TypeHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "no mcih type header, invalid packet");
      }
      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "removed type header, header type ")<< header.GetType());

      if( header.IsValid()){
        switch( header.GetType()){
          case MCIHTYPE_HELLO:
            ReceiveHello( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_MCHADV:
            ReceiveMchadv( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_SCHADV:
            NS_LOG_FUNCTION( Utility::Coloring( RED, "action for receiving schadv header is not implement yet"));
            break;
          case MCIHTYPE_CMADV:
            NS_LOG_FUNCTION( Utility::Coloring( RED, "action for receiving cmadv header is not implement yet"));
            break;
          case MCIHTYPE_UNADV:
            ReceiveUnadv( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_ELECTMCH:
            ReceiveElectMch( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_RGSTREQ:
            if( role== MasterClusterHead|| role== SubClusterHead) ReceiveRgstreq( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_RGSTREP:
            ReceiveRgstrep( packet, sender_address, interface, hoplimit);
            break;
          case MCIHTYPE_CHRESIGN:
            ReceiveResign( packet, sender_address, interface, hoplimit);
            break;
          default:
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "ignoring message with unknown type ")<< header.GetType());
        }
      }
      return;
    }

    void RoutingProtocol::ReceiveHello( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);

      HelloHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no hello header");
      }

      Ipv6Address addr= header.GetAddress();
      Vector pos= header.GetPosition();
      Vector vel= header.GetVelocity();
      RPM rpm= header.GetRelativePositionAndMobility();
      RSM rsm= header.GetRelativeStateAndMobility();
      Role role= header.GetRole();
      NS_LOG_LOGIC( "ROLE RECEIVE: "<< ToString( role));

      neighbor_nodes.Update( source, active_neighbor_timeout, header);
      if( cluster_members){
        cluster_members->Update( addr, active_neighbor_timeout, header);
      }

      if( role== MasterClusterHead|| role== SubClusterHead){
        neighbor_headers.Update( addr, active_neighbor_timeout, header, pos, vel);
      }

      if( role== MasterClusterHead|| role== SubClusterHead){
        NS_LOG_LOGIC( "cluster head received mchadv" );
      }

      //  bool AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag, Time lifetime);
      NS_ASSERT_MSG( source.IsLinkLocal(), "unadv packet comes from not link local address: "<< source);
      // routing_table.AddRoute( interface->GetDevice(), source, Ipv6Address::GetAllNodesMulticast(), interface->GetLinkLocalAddress(), VALID, active_route_timeout);

      Print( LOG_LEVEL_DEBUG, GREEN, pos);
      Print( LOG_LEVEL_DEBUG, GREEN, vel);
      // NS_LOG_LOGIC( " R P M : "<< rpm);
    }


    void RoutingProtocol::ReceiveUnadv( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      UnadvHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no unadv header");
      }
      neighbor_nodes.Update( source, active_neighbor_timeout, header);

      Vector pos= header.GetPosition();
      Vector vel= header.GetVelocity();
      RPM rpm= header.GetRelativePositionAndMobility();

      //  bool AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag, Time lifetime);
      NS_ASSERT_MSG( source.IsLinkLocal(), "unadv packet comes from not link local address: "<< source);
      // routing_table.AddRoute( interface->GetDevice(), source, Ipv6Address::GetAllNodesMulticast(), interface->GetLinkLocalAddress(), VALID, active_route_timeout);

      Print( LOG_LEVEL_DEBUG, GREEN, pos);
      Print( LOG_LEVEL_DEBUG, GREEN, vel);
      // NS_LOG_LOGIC( " R P M : "<< rpm);
    }


    void RoutingProtocol::ReceiveElectMch( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      ElectMchHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no elect mch header");
      }

      auto target_address= header.GetTargetAddress();
      NS_LOG_LOGIC( Utility::Coloring( GREEN, "Elected Master Cluster Head: ")<< target_address);

      if( IsOwnAddress( target_address)){
        SetRole( MasterClusterHead);
      }
    }

    void RoutingProtocol::ReceiveMchadv( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);

      MchadvHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no mchadv header");
      }
      auto mch_address= header.GetMchAddress();
      neighbor_headers.Update( mch_address, active_neighbor_timeout, header, position, velocity);

      Vector pos= header.GetPosition();
      Vector vel= header.GetVelocity();
      RPM rpm= header.GetRelativePositionAndMobility();

      if( role== MasterClusterHead|| role== SubClusterHead){
        NS_LOG_LOGIC( "cluster head received mchadv" );
      }
    }


    void RoutingProtocol::ReceiveRgstreq( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      RgstreqHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no mchadv header");
      }
      // neighbor_headers.Update( source, active_neighbor_timeout, header);

      // Vector pos= header.GetPosition();
      // Vector vel= header.GetVelocity();
      // RPM rpm= header.GetRelativePositionAndMobility();

      auto target_address= header.GetTargetAddress();
      auto request_address= header.GetRegistAddress();
      if( IsOwnAddress( target_address)){
        NS_LOG_LOGIC( Utility::Coloring( GREEN, "Registration Requst: ")<< request_address);
        NS_LOG_LOGIC( Utility::Coloring( GREEN, "invide to own cluster"));
        Simulator::ScheduleNow( &RoutingProtocol::SendRgstrep, this, request_address, request_address);//Ipv6Address::GetAllNodesMulticast(), request_address);
      }
      //  bool AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag, Time lifetime);
      NS_ASSERT_MSG( source.IsLinkLocal(), "mchadv packet comes from not link local address: "<< source);
      // routing_table.AddRoute( interface->GetDevice(), source, Ipv6Address::GetAllNodesMulticast(), interface->GetLinkLocalAddress(), VALID, active_route_timeout);
    }

    void RoutingProtocol::ReceiveRgstrep( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      RgstrepHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no mchadv header");
      }

      auto header_address= header.GetHeaderAddress();
      NS_LOG_LOGIC( "Cluster Head's Address: "<< header_address);

      if( !neighbor_headers.SetOwnClusterHead( header_address)){
        return;
      }

      mcih_routing_table.SetGateway( header_address);
      SetRole( ClusterMember);
    }

    void RoutingProtocol::ReceiveResign( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      ResignHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no chresign header");
      }

      Ipv6Address addr= header.GetHeaderAddress();

      if( neighbor_headers.IsNeighbor( addr)){
        NS_LOG_LOGIC( "neighbor headers resigned, therefore erasing from neighbor header list");
        neighbor_headers.DelEntry( addr);
      }

      if( neighbor_headers.IsOwnClusterHead( addr)){
        NS_LOG_LOGIC( "own cluster head resigned, therefore launching intercluster handover scheme");
        neighbor_headers.SetOwnClusterHead( Ipv6Address::GetAny());
        InterclusterHandover();
      }

      if( cluster_members){ // if ch is
        if( cluster_members->IsNeighbor( addr)){
          NS_LOG_LOGIC( "cluster member resigned from cluster, therefore removing from cluster members list");
          cluster_members->DelEntry( addr);
        }
      }
    }

    Ptr< Ipv6Route> RoutingProtocol::LoopbackRoute( const Ipv6Header &header, Ptr< NetDevice> output_interface) const{
      NS_LOG_FUNCTION( this<< Utility::Coloring( RED, "do not use this function"));
      NS_LOG_LOGIC( Utility::Coloring( CYAN, header));
      if( !loopback_device) throw invalid_argument( "loopback device has not instanced yet.");

      auto route= Create< Ipv6Route>();
      route->SetDestination( header.GetDestinationAddress());

      auto itr= socket_interfaces.begin();
      if( output_interface){ // is there the output interface?
        bool is_interface= false;
        for( itr= socket_interfaces.begin(); itr!= socket_interfaces.end(); itr++){

          auto l3= ipv6->GetObject< Ipv6L3Protocol>();
          auto interface= itr->second;
          auto address= interface->GetLinkLocalAddress().GetAddress();
          auto index= ipv6->GetInterfaceForAddress( address);
          auto device= ipv6->GetNetDevice( index);
          if( output_interface== ipv6->GetNetDevice( index)){
            is_interface= true;
            route->SetSource( address);
            break;
          }
        } // end for

        if( is_interface){ // is interface found?
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "interface is found"));
        } else{
          throw invalid_argument( Utility::Coloring( MAGENTA, "interface is not found"));
        }
      } else{ // output interface is not specified
        NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "interface is not specified"));
        route->SetSource( itr->second->GetLinkLocalAddress().GetAddress());
      }

      NS_ASSERT_MSG( route->GetSource()!= Ipv6Address(), "calid source address not found");
      route->SetGateway( Ipv6Address::GetLoopback());
      route->SetOutputDevice( loopback_device);
      //NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      return route;
    }

    void RoutingProtocol::RoleCheckTimerExpire(){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet")<< ToString( role));

      // NS_LOG_DEBUG( Utility::Coloring( GREEN, "check routing table"));
      // routing_table.Print( LOG_LEVEL_DEBUG);

      // NS_LOG_DEBUG( Utility::Coloring( GREEN, "check interfaces")<< ": "<< socket_interfaces.size());
      for_each( socket_interfaces.begin(), socket_interfaces.end(), [&]( auto factor){
          auto socket= factor.first;
          auto interface= factor.second;
          // NS_LOG_LOGIC( interface);
          // Print( LOG_LEVEL_LOGIC, GREEN, interface );
          });

      // Print( LOG_LEVEL_LOGIC, GREEN, neighbor_nodes.GetNdiscCache());

      auto l3= ipv6->GetObject< Ipv6L3Protocol>();
      auto interface= l3->GetInterface( 1);

      SendHello();

      switch( role){
        case Undecided:
          if( default_role!= role) SetRole( default_role);
          interface->SetForwarding( false);
          if( neighbor_headers.GetNeighborNumber()){
            SendRgstreq( neighbor_headers.GetLowestRpmNeighborAddress());// Ipv6Address::GetAllRoutersMulticast()); //neighbor_headers.GetLowestRpmNeighborAddress());
          }
          break;

        case ClusterMember:
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member is not implement yet"));
          InterclusterHandover();
          break;

        case SubClusterHead:
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head is not implement yet"));
          break;

        case MasterClusterHead:
          NS_LOG_LOGIC( "role is master cluster head");
          neighbor_headers.SetOwnClusterHead( Ipv6Address::GetAny());
          if( interface){
            interface->SetForwarding( true);
          } else{
            NS_LOG_LOGIC( Utility::Coloring( RED, "what is happend? interface was not found."));
          }
          if( cluster_members){ // is have some cluster member
            NS_LOG_FUNCTION("cluster size: "<< cluster_members->GetNeighborNumber());
          } else{
            cluster_members= unique_ptr< ClusterMembers>( new ClusterMembers( hello_interval));
          }
          if( cluster_members->GetNeighborNumber()){ // is have some cluster member
            EmptyCheckUpdate( contention_interval); // to extend the timer.
          }
          cluster_members->Print();
          //SendMchadv( Ipv6Address::GetAllNodesMulticast());
          break;

        case RoleNumber:
          NS_ABORT_MSG( "invalid role");
          break;
      }

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "set next timer: ")<< role_check_interval.As( Time::Unit::MS));
      role_check_timer.Schedule( role_check_interval);
    }

    void RoutingProtocol::VelocityCheckTimerExpire(){
      // NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      Ptr< MobilityModel> mobility_model= GetMobilityModel();
      velocity= GetDistance( mobility_model->GetPosition(), position);
      position= mobility_model->GetPosition();
      velocity.x= velocity.x/ velocity_check_interval.GetSeconds();
      velocity.y= velocity.y/ velocity_check_interval.GetSeconds();
      // Print( LOG_LEVEL_LOGIC, GREEN, position);
      // Print( LOG_LEVEL_LOGIC, GREEN, velocity);
      velocity_check_timer.Schedule( velocity_check_interval);
    }

    void RoutingProtocol::ElectMchTimerExpire(){
      if( role!= Undecided){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "electing is canceled, because role is not undecided"));
        return ;
      }
      if( neighbor_nodes.GetNeighborNumber()){
        NS_LOG_FUNCTION( Utility::Coloring( CYAN, "electing master cluster head"));
        //SendElectMch( Ipv6Address::GetAllNodesMulticast());//neighbor_nodes.GetHighestRpmNeighborAddress());//Ipv6Address::GetAllNodesMulticast());
        SendElectMch( Ipv6Address::GetAllRoutersMulticast());//neighbor_nodes.GetHighestRpmNeighborAddress());//Ipv6Address::GetAllNodesMulticast());
        //SendElectMch( neighbor_nodes.GetLowestRpmNeighborAddress());
      }
      ElectMchUpdate(elect_mch_interval);
    }

    void RoutingProtocol::EmptyCheckTimerExpire(){
      if( role== MasterClusterHead| role== SubClusterHead){
        NS_LOG_FUNCTION( Utility::Coloring( RED, "cluster is empty, therefore resign master cluster head. and trying to join other cluster"));
        SendResign( Ipv6Address::GetAllNodesMulticast());
        SetRole( Undecided);
      }
      empty_check_timer.Cancel();
    }

    void RoutingProtocol::SendTo( Ptr< Socket> socket, Ptr< Packet> packet, Ipv6Address destination){
      NS_LOG_FUNCTION( this<< packet->GetUid());
      auto interface= FindInterface( socket);
      auto if_index= ipv6->GetInterfaceForDevice( interface->GetDevice());
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "send to ")<< destination<< ", from if: "<< if_index<< "("<< interface<<")");
      int ret= socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
      if( ret< 0){
        NS_LOG_LOGIC( Utility::Coloring( RED, " * * * * * * * * * * * * * * "));
        NS_LOG_LOGIC( Utility::Coloring( RED, "  E R R O R   O N   S E N D "));
        NS_LOG_LOGIC( Utility::Coloring( RED, " * * * * * * * * * * * * * * "));
      } else{
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "send packet is completed, and packet size is ")<< ret);
      }
    }

    void RoutingProtocol::DoDispose(){
      NS_LOG_FUNCTION( this);
      if( role!= Undecided){
        connectable.push_back( Simulator::Now()- become_connectable_time);
      }

      for( auto itr= connectable.begin(); itr!= connectable.end(); itr++){
        NS_LOG_FUNCTION( distance( connectable.begin(), itr)<< (*itr).GetMilliSeconds()/ 1000.0);
      }
    }

    void RoutingProtocol::DoInitialize(){
      NS_LOG_FUNCTION( this);
      initialized= true;

      for( int if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        if( !if_index) continue;
        auto device= ipv6->GetNetDevice( if_index);
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        auto interface= l3->GetInterface( if_index);
        if( !FindReceiveSocket( interface)){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "create receiving socket"));
          auto tid= TypeId::LookupByName( "ns3::UdpSocketFactory");
          auto node= ipv6->GetObject< Node>();
          auto receive_socket= Socket::CreateSocket( node, tid);
          auto local= Inet6SocketAddress( Ipv6Address::GetAny(), MCIH_PORT);
          receive_socket->Bind( local);
          receive_socket->SetRecvCallback( MakeCallback( &RoutingProtocol::ReceiveCallback, this));
          receive_socket->SetIpv6RecvHopLimit( true);
          receive_socket->SetRecvPktInfo( true);
          receive_socket_interfaces.insert( make_pair( receive_socket, interface));
        }
      }

      bool has_global= false;
      for( uint32_t if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        bool active_interface= interface_exclusions.find( if_index)== interface_exclusions.end();
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        auto interface= l3->GetInterface( if_index);
        for( uint32_t ad_index= 0; ad_index< interface->GetNAddresses(); ad_index++){
          auto address= interface->GetAddress( ad_index);
          if( address.GetScope()== Ipv6InterfaceAddress::LINKLOCAL&& active_interface){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "MCIH: adding socket to ") << address.GetAddress ());
            TypeId tid= TypeId::LookupByName( "ns3::UdpSocketFactory");
            auto node= GetObject< Node> ();
            auto socket= Socket::CreateSocket( node, tid);
            auto local= Inet6SocketAddress( address.GetAddress(), MCIH_PORT);
            int ret= socket->Bind( local);
            NS_ASSERT_MSG( ret== 0, "could not bind");
            socket->BindToNetDevice( ipv6->GetNetDevice( if_index));
            socket->ShutdownRecv();
            socket->SetIpv6RecvHopLimit (true);
            // m_sendSocketList[socket] = i;
            socket_interfaces.insert( make_pair( socket, interface));
          } else if( address.GetScope()== Ipv6InterfaceAddress::GLOBAL){
            has_global= true;
          }
        }
      }

      if( has_global){
        NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "global address is had, but not implementation yet"));
      }

      NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "scheduling is not implementation yet"));

      Ipv6RoutingProtocol::DoInitialize();
    }

    void RoutingProtocol::AddNetworkRouteTo( Ipv6Address network, Ipv6Prefix networkPrefix, uint32_t interface){
      NS_LOG_FUNCTION (this << network << networkPrefix << interface);

      Ptr< McihRoutingTableEntry> route = Create< McihRoutingTableEntry>(network, networkPrefix, interface);
      route->SetRouteMetric( 1);
      route->SetRouteStatus( McihRoutingTableEntry::McihValid);
      route->SetRouteChanged( true);

      mcih_routing_table.AddRoute( route, EventId());
    }

    void RoutingProtocol::SendTriggeredRouteUpdate(){
      NS_LOG_FUNCTION (this);

      /*
         if( m_nextTriggeredUpdate.IsRunning()){
         NS_LOG_LOGIC ("Skipping Triggered Update due to cooldown");
         return;
         }
         Time delay = Seconds (m_rng->GetValue (m_minTriggeredUpdateDelay.GetSeconds (), m_maxTriggeredUpdateDelay.GetSeconds ()));
         m_nextTriggeredUpdate = Simulator::Schedule (delay, &RipNg::DoSendRouteUpdate, this, false);
         */
    }

    void RoutingProtocol::DoSendRouteUpdate( bool periodic){
      NS_LOG_FUNCTION( this<< ( periodic? "periodic": "triggered"));
      NS_LOG_LOGIC( Utility::Coloring( RED, "updating route is not implementation yet."));
      /*
         for( auto itr= socket_interfaces.begin(); itr!= socket_interfaces.end(); itr++){
         auto interface= itr->second;
         auto if_index= ToIndex( interface);
         if( interface_exclusions.find( if_index)== interface_exclusions.end()){
         uint16_t mtu= ipv6->GetMtu( if_index);
         uint16_t max_rte= mtu- Ipv6Header().GetSerializedSize()- UdpHeader().GetSerializedSize()- TypeHeader().GetSerializedSize(); // bug
         Ptr< Packet> packet= Create< Packet>();
         SocketIpv6HopLimitTag tag;
         tag.SetHopLimit( 255);

         TypeHeader type;
         }
         }
         */
    }


    bool RoutingProtocol::IsOwnAddress( Ipv6Address address){
      for( size_t if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++)
        for( size_t ad_index= 0; ad_index< ipv6->GetNAddresses( if_index); ad_index++){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, address)<< " == "<<Utility::Coloring( CYAN, ipv6->GetAddress( if_index, ad_index)));
          if( address== ipv6->GetAddress( if_index, ad_index).GetAddress())
            return true;
        }
      return false;
    }

    void RoutingProtocol::InterclusterHandover(){
      NS_LOG_FUNCTION( this);
      auto best= neighbor_headers.GetBestHeader();
      if( !neighbor_headers.IsOwnClusterHead( best.neighbor_address)){ // searching new cluster head
        auto och= neighbor_headers.GetOwnClusterHead();
        NS_LOG_LOGIC( Utility::Coloring( RED, "Handover")
            << " from "<< och.neighbor_address<< "("<< och.rsm<< ")"
            << " to "<< best.neighbor_address<< "("<< best.rsm<<")");
        SendResign( neighbor_headers.GetOwnClusterHead().neighbor_address);
        SendRgstreq( best.neighbor_address);
      }
    }

    void RoutingProtocol::EmptyCheckUpdate( Time time){
      NS_LOG_FUNCTION( this<< time.GetMilliSeconds()/1000.0);
      if( empty_check_timer.IsRunning())
        empty_check_timer.Cancel();
      empty_check_timer.Schedule( time);
    }

    void RoutingProtocol::ElectMchUpdate( Time time){
      NS_LOG_FUNCTION( this<< time.GetMilliSeconds()/1000.0);
      if( elect_mch_timer.IsRunning())
        elect_mch_timer.Cancel();
      elect_mch_timer.Schedule( time);
    }

    void Print( LogLevel level, Color color, Ipv6Address address){
      NS_LOG( level, Utility::Coloring( color, "address")<< ": "<< address
          << ", "<< string( Utility::Coloring( color, "is link local"))<< ": "<< ( address.IsLinkLocal()? "true": "false ")
          << ", "<< string( Utility::Coloring( color, "is link local multicast"))<< ": "<< ( address.IsLinkLocalMulticast()? "true": "false ")
          );
    }

    void Print( LogLevel level, Color color, Ipv6InterfaceAddress address){
      NS_LOG( level, Utility::Coloring( color, "address")<< ": "<< address.GetAddress()
          << ", "<< string( Utility::Coloring( color, "prefix"))<< ": "<< address.GetPrefix()
          << ", "<< string( Utility::Coloring( color, "scope"))<< ": "<< Utility::Scope( address.GetScope())
          );
    }

    void Print( LogLevel level, Color color, Ptr< Ipv6Interface> interface){
      NS_LOG( level, string( Utility::Coloring( color, "address number"))<< ": "<< interface->GetNAddresses()
          << ", "<< string( Utility::Coloring( color, "metric"))<< ": "<< interface->GetMetric()
          << ", "<< string( Utility::Coloring( color, "state"))<< ": "<< ( interface->IsUp()?"UP":"DOWN")
          << ", "<< string( Utility::Coloring( color, "forwarding"))<< ": "<< ( interface->IsForwarding()?"forwarding":"unforward")
          );
      for( int index= 0; index< interface->GetNAddresses(); index++){
        Print( level, color, interface->GetAddress( index));
      }
    }

    void Print( LogLevel level, Color color, Ptr< Socket> socket){
      NS_LOG( level, Utility::Coloring( color, "allow broadcast")<< ": "<< ( socket->GetAllowBroadcast()? "true":"false" )
          << ", "<< string( Utility::Coloring( color, "ttl"))<< ": "<< socket->GetIpTtl()
          << ", "<< string( Utility::Coloring( color, "hop limit"))<< ": "<< socket->GetIpv6HopLimit()
          << ", "<< string( Utility::Coloring( color, "tx"))<< ": "<< socket->GetTxAvailable()
          << ", "<< string( Utility::Coloring( color, "rx"))<< ": "<< socket->GetRxAvailable()
          );
    }

    static void Print( LogLevel level, Color color, std::vector< Ptr< NdiscCache> > cache){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "can not use"));
      NS_LOG( level, Utility::Coloring( color, "ndisc cache")<< ": "<< cache.size());
      cache.begin();
      for_each( cache.begin(), cache.end(), [ &]( auto factor){
          NS_LOG_LOGIC( factor);
          });
    }

    static void Print( LogLevel level, Color color, Vector vec){
      static char msg[ 127];
      sprintf( msg, "%7.2lf, %7.2lf, %7.2lf", vec.x, vec.y, vec.z);
      NS_LOG( level, Utility::Coloring( color, "Vector")<< ": "<< msg);
    }

    static void Print( LogLevel level, Color color, Ptr< Ipv6Route> route){
      NS_LOG( level, string( Utility::Coloring( color, "destination"))<< ": "<< route->GetDestination()<< ", "
          << string( Utility::Coloring( color, "gateway"))<< ": "<< route->GetGateway()<< ", "
          << string( Utility::Coloring( color, "device"))<< ": "<< route->GetOutputDevice()<< ", "
          << string( Utility::Coloring( color, "source"))<< ": "<< route->GetSource()
          );
    }

  }
} 

