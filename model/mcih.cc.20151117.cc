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

#include "mcih.h"
#include "mcih-utility.h"

using namespace std;

namespace ns3{
  NS_LOG_COMPONENT_DEFINE( "McihRoutingProtocol");
  namespace mcih{
    NS_OBJECT_ENSURE_REGISTERED( RoutingProtocol);
    const uint32_t RoutingProtocol::MCIH_PORT= 1701;

    class DeferredRouteOutputTag: public Tag{
      public:
        DeferredRouteOutputTag( int32_t index= -1): Tag(), if_index( index){
        }
        static TypeId GetTypeId(){
          static TypeId tid= TypeId( "ns3::mcih::DeferredRouteOutputTag").SetParent<Tag>()
            .SetParent<Tag>()
            .SetGroupName( "Mcih")
            .AddConstructor< DeferredRouteOutputTag>()
            ;
          return tid;
        }
        TypeId GetInstanceTypeId() const{
          return GetTypeId();
        }
        virtual void  Deserialize( TagBuffer tag){
          if_index= tag.ReadU32();
        }
        virtual uint32_t  GetSerializedSize (void) const{
          return sizeof( if_index);
        }
        virtual void  Print (std::ostream &os) const{
        }
        virtual void  Serialize( TagBuffer tag) const{
          tag.WriteU32( if_index);
        }
      private:
        int32_t if_index;
    };
    NS_OBJECT_ENSURE_REGISTERED( DeferredRouteOutputTag);

    RoutingProtocol::RoutingProtocol():
      ipv6(),
      node(),
      loopback_device(),
      socket_addresses(),
      role( Undecided),
      hello_interval( Seconds( 1)),
      active_route_timeout( Seconds( 3)),
      active_neighbor_timeout( Seconds( 3)),
      role_check_interval( Seconds( 1)), 
      velocity_check_interval( MilliSeconds( 100)),
      elect_mch_interval( Seconds( 5)),
      role_check_timer( Timer::CANCEL_ON_DESTROY),
      velocity_check_timer( Timer::CANCEL_ON_DESTROY),
      elect_mch_timer( Timer::CANCEL_ON_DESTROY),
      routing_table(), 
      neighbor_nodes( hello_interval),
      neighbor_headers( hello_interval),
      interface_exclusions(),
      receive_socket(),
      uniform_random_variable( new UniformRandomVariable){
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
      if( !l3->IsUp( if_index)){
        NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, "interface is not up"));
        return;
      }

      if( interface_exclusions.find( if_index)!= interface_exclusions.end()){
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
        // AddNetworkRouteTo( network_address, network_prefix, if_index);
      }

      // SendTriggeredRouteUpdate();
    }

    void RoutingProtocol::NotifyAddRoute( Ipv6Address dst, Ipv6Prefix mask, Ipv6Address next_hop, uint32_t if_num, Ipv6Address prefix_to_use){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "destination: ")<< dst);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "prefix: ")<< mask);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "next hop ")<< next_hop);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "interface index: ")<< if_num);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "prefix to use: ")<< prefix_to_use);
    }

    void RoutingProtocol::NotifyInterfaceDown( uint32_t if_num){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::NotifyInterfaceUp( uint32_t if_num){
      NS_LOG_FUNCTION( this);
      auto l3= ipv6->GetObject< Ipv6L3Protocol>();
      auto interface= l3->GetInterface( if_num);
      interface->SetForwarding( true);
      auto device= l3->GetNetDevice( if_num);

      Print( LOG_LEVEL_LOGIC, GREEN, interface);
      for( int index= 0; index< interface->GetNAddresses(); index++){
        auto if_address= interface->GetAddress( index);
        if( if_address.GetScope()== Ipv6InterfaceAddress::HOST){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "interface is for localhost"));
          continue;
        }

        auto socket= FindSocket( if_address);
        if( !socket){ // if could not found a socket
          auto socket= Socket::CreateSocket( GetObject< Node>(), UdpSocketFactory::GetTypeId());
          NS_ASSERT( socket!= 0);
          socket->SetRecvCallback( MakeCallback( &RoutingProtocol::ReceiveCallback, this));
          auto local= Inet6SocketAddress( Ipv6Address::GetAny(), MCIH_PORT);
          socket->Bind( local);
          // socket->Bind6();//( Inet6SocketAddress( Ipv6Address::GetAny(), MCIH_PORT));
          socket->SetIpv6RecvHopLimit( true);
          socket->BindToNetDevice( device);
          // socket->SetAllowBroadcast( true);
          // socket->SetIpTtl( 1);

          socket_interfaces.insert( std::make_pair( socket, interface));
          socket_addresses.insert( std::make_pair( socket, if_address));
        }
      }

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "add route for link local to routing table"));
      // bool RoutingTable::AddRoute( Ptr< NetDevice> device, Ipv6Address destination, Ipv6Address gateway, Ipv6InterfaceAddress interface, RouteFlags flag);
      routing_table.AddRoute( device, Ipv6Address::GetAllNodesMulticast(), Ipv6Address::GetAllNodesMulticast(), interface->GetLinkLocalAddress(), VALID, Simulator::Now()+ Time( Seconds( 3000)));
      routing_table.AddRoute( device, Ipv6Address::GetAllRoutersMulticast(), Ipv6Address::GetAllRoutersMulticast(), interface->GetLinkLocalAddress(), VALID, Simulator::Now()+ Time( Seconds( 3000)));
      routing_table.AddRoute( device, Ipv6Address::GetAllHostsMulticast(), Ipv6Address::GetAllHostsMulticast(), interface->GetLinkLocalAddress(), VALID, Simulator::Now()+ Time( Seconds( 3000)));
      routing_table.Print( LOG_LEVEL_LOGIC);

      auto ndisc= interface->GetNdiscCache();
      if( ndisc){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "interface has ndisc cache: ")<< ndisc);
        neighbor_nodes.AddNdiscCache( ndisc);
      }

      auto wifi= device->GetObject< WifiNetDevice>();
      if( !wifi){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "could not get wifi object"));
        return;
      }

      auto mac= wifi->GetMac();
      if( !mac){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "could not get mac address from wifi object"));
        return;
      }

      mac->TraceConnectWithoutContext( "TxErrHeader", neighbor_nodes.GetTxErrorCallback());
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "set tx error callback"));

      if( !receive_socket){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "create receiving socket"));
        auto tid= TypeId::LookupByName( "ns3::UdpSocketFactory");
        auto node= GetObject< Node>();
        receive_socket= Socket::CreateSocket( node, tid);
        auto local= Inet6SocketAddress( Ipv6Address::GetAny(), MCIH_PORT);
        receive_socket->Bind( local);
        receive_socket->SetRecvCallback( MakeCallback( &RoutingProtocol::ReceiveCallback, this));
        receive_socket->SetIpv6RecvHopLimit( true);
        receive_socket->SetRecvPktInfo( true);
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
      NS_LOG_FUNCTION( this);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, header)<< " -  "<< Utility::Coloring( CYAN, "interface: ")<< (output_interface? output_interface->GetIfIndex(): 0));

      if( !packet){ // is there the packet instance?
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "packet is null, so delegate to loopback route"));
        return LoopbackRoute( header, output_interface);
      }

      NS_LOG_LOGIC( Utility::Coloring( CYAN, "socket address: ")<< socket_addresses.size());
      if( socket_addresses.empty()){ // does the map have any sockets?
        sockerr= Socket::ERROR_NOROUTETOHOST;
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "mcih has no interfaces"));
        return Ptr< Ipv6Route>();
      }

      // print out sockets for debug
      for_each( socket_addresses.begin(), socket_addresses.end(), []( auto element){
          Print( LOG_LEVEL_LOGIC, GREEN, element.first);
          Print( LOG_LEVEL_LOGIC, GREEN, element.second);
          });

      sockerr= Socket::ERROR_NOTERROR;
      Ptr< Ipv6Route> route;
      auto entry= routing_table.GetVanillaEntry(); // instancing new entry
      auto destination= header.GetDestinationAddress();
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "searching valid route to ")<< destination);
      if( routing_table.LookupValidRoute( destination, *entry)){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "route is exist"));
        route= entry->GetRoute();
        NS_ASSERT( route!= 0);

        // there is route, but there is no device.
        if( output_interface!= 0&& route->GetOutputDevice()!= output_interface){
          sockerr= Socket::ERROR_NOROUTETOHOST;
          return Ptr< Ipv6Route>();
        }

        // updating the lifetime of each routes
        routing_table.UpdateRouteLifetime( destination, active_route_timeout);
        routing_table.UpdateRouteLifetime( route->GetGateway(), active_route_timeout);
        return route;
      }

      NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "route is not exist"));
      int32_t input_interface= ( output_interface? ipv6->GetInterfaceForDevice( output_interface): -1);
      DeferredRouteOutputTag tag( input_interface);
      if( !packet->PeekPacketTag( tag)){
        packet->AddPacketTag( tag);
      }
      return LoopbackRoute( header, output_interface);Ptr< Ipv6Route>();
    }

    bool RoutingProtocol::RouteInput( Ptr< const Packet> packet, const Ipv6Header &header, Ptr< const NetDevice> device, UnicastForwardCallback unicast_callback, MulticastForwardCallback multicast_callback, LocalDeliverCallback local_callback, ErrorCallback error_callback){
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, packet->GetUid()));
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "destination: ")<< header.GetDestinationAddress()
          << ", "<< "device: "<< device->GetAddress());
      if( socket_addresses.empty()){
        NS_LOG_LOGIC( Utility::Coloring( RED, "no interface" ));
        return false;
      }
      NS_ASSERT( ipv6!= 0);
      NS_ASSERT( packet!= 0);
      NS_ASSERT( ipv6->GetInterfaceForDevice( device)>= 0);
      int32_t if_index= ipv6->GetInterfaceForDevice( device);

      auto destination= header.GetDestinationAddress();
      auto source= header.GetSourceAddress();

      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      if( device== loopback_device){
        DeferredRouteOutputTag tag;
        if( packet->PeekPacketTag( tag)){
          //DeferredRouteOutput( packet, header, unicast_callback, error_callback);
          return true;
        }
      }
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::SetIpv6( Ptr< Ipv6> arg_ipv6){
      NS_LOG_FUNCTION( this);
      if( !arg_ipv6) throw invalid_argument( "arg ipv6 is null.");
      if( ipv6) throw invalid_argument( "ipv6 is not null.");

      ipv6= arg_ipv6;
      int if_index= 0;
      int addr_index= 0;
      NS_LOG_LOGIC( Utility::Coloring( CYAN, ipv6->GetAddress( if_index, addr_index)));
      loopback_device= ipv6->GetNetDevice( if_index);
      if( !loopback_device) throw invalid_argument( "could not get loopback device");
      auto l3= ipv6->GetObject< Ipv6L3Protocol>();
      auto interface= l3->GetInterface( if_index);
      auto if_address= interface->GetAddress( addr_index);
      auto address= if_address.GetAddress();

      if( address!= Ipv6Address::GetLoopback()){
        throw invalid_argument( "loopback device is not found");
      }
      if( ipv6->GetNInterfaces()!= 1){
        throw invalid_argument( "too many interfaces. target is only loopback device");
      }
      /*
         for( int if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
         for( int addr_num= 0; addr_num< ipv6->GetNAddresses( if_index); addr_num++){
         }
         }
         */

      // add routing table entry
      //auto address= ipv6->GetAddress( if_index, addr_num).GetAddress();
      //auto interface= Ipv6InterfaceAddress( address);
      auto flag= VALID;
      //routing_table.AddRoute( device, Ipv6Address::GetAllRoutersMulticast(), Ipv6Address::GetAny(), interface, VALID);
      routing_table.AddRoute( loopback_device, Ipv6Address::GetLoopback(), Ipv6Address::GetLoopback(), if_address, VALID, Simulator::GetMaximumSimulationTime());

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

    void RoutingProtocol::SendUnadv( Ipv6Address destination){
      NS_LOG_FUNCTION( Utility::Coloring( MAGENTA, this));
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "destination: ")<< destination);
      if( !destination.IsLinkLocalMulticast()) throw invalid_argument( "undecided advertisement is only link local multicast" );

      SocketIpv6HopLimitTag hoplimit_tag;
      TypeHeader type( MCIHTYPE_UNADV);
      UnadvHeader unadv;
      unadv.SetPosition( position);
      unadv.SetVelocity( velocity);
      unadv.SetRelativePositionAndMobility( GetRPM());
      auto packet= Create< Packet>();
      //packet->RemovePacketTag( hoplimit_tag);
      hoplimit_tag.SetHopLimit( 255);
      packet->AddPacketTag( hoplimit_tag);
      packet->AddHeader( unadv);
      packet->AddHeader( type);

      for( auto ad_itr= socket_addresses.begin(); ad_itr!= socket_addresses.end(); ad_itr++){
        auto socket= ad_itr->first;
        auto address= ad_itr->second.GetAddress();
        if( !address.IsLinkLocal()) continue;
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
    }

    void RoutingProtocol::SendElectMch( Ipv6Address destination){
      NS_LOG_FUNCTION( this);
      SocketIpv6HopLimitTag hoplimit_tag;
      TypeHeader type( MCIHTYPE_ELECTMCH);
      ElectMchHeader electmch;
      auto packet= Create< Packet>();
      hoplimit_tag.SetHopLimit( 255);
      packet->AddPacketTag( hoplimit_tag);
      packet->AddHeader( electmch);
      packet->AddHeader( type);


      for( auto ad_itr= socket_addresses.begin(); ad_itr!= socket_addresses.end(); ad_itr++){
        auto socket= ad_itr->first;
        auto address= ad_itr->second.GetAddress();
        if( !address.IsLinkLocal()) continue;
        Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
      }
      /*
      for( auto if_itr= socket_interfaces.begin(); if_itr!= socket_interfaces.end(); if_itr++){
        auto socket= if_itr->first;
        auto interface= if_itr->second;
        auto address= interface->GetLinkLocalAddress();
        Print( LOG_LEVEL_LOGIC, CYAN, interface);
        // Print( LOG_LEVEL_LOGIC, CYAN, destination);
        NS_LOG_FUNCTION( address);
        destination= Ipv6Address::GetAllRoutersMulticast();
        SendTo( socket, packet, destination);
        //Simulator::Schedule( Time( MilliSeconds( 5)), &RoutingProtocol::SendTo, this, socket, packet, destination);
        break;
        // SendTo( socket, packet, destination);
      }
      */
    }

    void RoutingProtocol::SetRole( Role r){
      if( role== r) return;
      switch( role){
        case Undecided:
          if( r== ClusterMember){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "undecided -> cluster member"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else throw invalid_argument( "invalid updating role to without cluster member from undecided");
          break;
        case ClusterMember:
          if( r== Undecided){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> undecided"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
            elect_mch_timer.Schedule( elect_mch_interval);
          } else if( r==SubClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> sub cluster head"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else if( r==MasterClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member -> master cluster head"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else throw invalid_argument( "invalid role");
          break;
        case SubClusterHead:
          if( r== Undecided){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> undecided"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
            elect_mch_timer.Schedule( elect_mch_interval);
          } else if( r== ClusterMember){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> cluster member"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else if( r== MasterClusterHead){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head -> master cluster head"));
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "not implement yet"));
          } else throw invalid_argument( "invalid role");
          break;
        case MasterClusterHead:
          break;
        case RoleNumber:
          throw invalid_argument( "invalid role");
          break;
      }
      role= r;
    }

    void RoutingProtocol::Start(){
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "node launch"));
      role_check_timer.SetFunction( &RoutingProtocol::RoleCheckTimerExpire, this);
      role_check_timer.Schedule( role_check_interval);
      velocity_check_timer.SetFunction( &RoutingProtocol::VelocityCheckTimerExpire, this);
      velocity_check_timer.Schedule( velocity_check_interval);
      elect_mch_timer.SetFunction( &RoutingProtocol::ElectMchTimerExpire, this);
      elect_mch_timer.Schedule( elect_mch_interval);
    }

    Ptr< Socket> RoutingProtocol::FindSocket( Ipv6InterfaceAddress address) const{
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, Utility::InterfaceAddress( address)));
      for( auto itr= socket_addresses.begin(); itr!= socket_addresses.end(); itr++){
        auto socket= itr->first;
        auto interface= itr->second;
        if( interface== address) return socket;
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

    void RoutingProtocol::ReceiveCallback( Ptr< Socket> socket){
      NS_LOG_FUNCTION( this);//Utility::Coloring( RED, "not implement yet"));
      auto packet= socket->Recv();
      NS_LOG_INFO( "Received "<< *packet);

      // Ipv6PacketInfoTag packet_info;
      // if( !packet->RemovePacketTag( packet_info)){
      // NS_ABORT_MSG( "No incoming interface on RIPng message, aborting.");
      // }
      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "removed packet information tag"));
      // NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "comment out for debug"));

      auto interface= FindInterface( socket);
      if( !interface){
        NS_ABORT_MSG( "unknown interface is set in receive callback");
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
          case MCIHTYPE_DUMMY:
            NS_LOG_FUNCTION( Utility::Coloring( RED, "action for receiving dummy header is not implement yet"));
            break;
          case MCIHTYPE_MCHADV:
            NS_LOG_FUNCTION( Utility::Coloring( RED, "action for receiving mchadv header is not implement yet"));
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
            // NS_LOG_LOGIC( "Hello World!!" );
            ReceiveElectMch( packet, sender_address, interface, hoplimit);
            break;
          default:
            NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "ignoring message with unknown type ")<< header.GetType());
        }
      }
      return;
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
      routing_table.AddRoute( interface->GetDevice(), source, Ipv6Address::GetAllNodesMulticast(), interface->GetLinkLocalAddress(), VALID, active_route_timeout);

      // Print( LOG_LEVEL_LOGIC, GREEN, pos);
      // Print( LOG_LEVEL_LOGIC, GREEN, vel);
      // NS_LOG_LOGIC( " R P M : "<< rpm);
    }


    void RoutingProtocol::ReceiveElectMch( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit){
      NS_LOG_FUNCTION( this<< source);
      //NS_LOG_LOGIC( Utility::Coloring( CYAN, "receive from ")<< source);

      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implementation yet, to need updating routing table entry"));

      ElectMchHeader header;
      if( !packet->RemoveHeader( header)){
        NS_ABORT_MSG( "packet has no elect mch header");
      }

      // Print( LOG_LEVEL_LOGIC, GREEN, pos);
      // Print( LOG_LEVEL_LOGIC, GREEN, vel);
      // NS_LOG_LOGIC( " R P M : "<< rpm);
    }

    Ptr< Ipv6Route> RoutingProtocol::LoopbackRoute( const Ipv6Header &header, Ptr< NetDevice> output_interface) const{
      NS_LOG_FUNCTION( this);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, header));
      if( !loopback_device) throw invalid_argument( "loopback device has not instanced yet.");

      auto route= Create< Ipv6Route>();
      route->SetDestination( header.GetDestinationAddress());

      auto itr= socket_addresses.begin();
      if( output_interface){ // is there the output interface?
        bool is_interface= false;
        for( itr= socket_addresses.begin(); itr!= socket_addresses.end(); itr++){

          auto address= itr->second.GetAddress();
          uint32_t index= ipv6->GetInterfaceForAddress( address);
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
        route->SetSource( itr->second.GetAddress());
      }

      NS_ASSERT_MSG( route->GetSource()!= Ipv6Address(), "calid source address not found");
      route->SetGateway( Ipv6Address::GetLoopback());
      route->SetOutputDevice( loopback_device);
      //NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      return route;
    }

    void RoutingProtocol::RoleCheckTimerExpire(){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));

      NS_LOG_DEBUG( Utility::Coloring( GREEN, "check routing table"));
      routing_table.Print( LOG_LEVEL_DEBUG);

      // NS_LOG_DEBUG( Utility::Coloring( GREEN, "check interfaces")<< ": "<< socket_interfaces.size());
      for_each( socket_interfaces.begin(), socket_interfaces.end(), [&]( auto factor){
          auto socket= factor.first;
          auto interface= factor.second;
          // NS_LOG_LOGIC( interface);
          // Print( LOG_LEVEL_LOGIC, GREEN, interface );
          });

      // Print( LOG_LEVEL_LOGIC, GREEN, neighbor_nodes.GetNdiscCache());

      switch( role){
        case Undecided:
          //SendUnadv( Ipv6Address::GetAllNodesMulticast());
          SendUnadv( Ipv6Address::GetAllRoutersMulticast());
          //SendUnadv( Ipv6Address::GetAllHostsMulticast());
          //SendUnadv( Ipv6Address::GetLoopback());
          break;
        case ClusterMember:
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "cluster member is not implement yet"));
          break;
        case SubClusterHead:
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "sub cluster head is not implement yet"));
          break;
        case MasterClusterHead:
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "master cluster head is not implement yet"));
          break;
        case RoleNumber:
          throw invalid_argument( "invalid role");
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
      NS_LOG_FUNCTION( Utility::Coloring( CYAN, "electing master cluster head"));
      if( role!= Undecided) NS_LOG_LOGIC( Utility::Coloring( CYAN, "electing is canceled, because role is not undecided"));
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
      
      //routing_table.Get
      NS_LOG_LOGIC( "test test "<< neighbor_nodes.GetHighestRpmNeighborAddress());
      SendElectMch( neighbor_nodes.GetHighestRpmNeighborAddress());//Ipv6Address::GetAllNodesMulticast());
    }

    void RoutingProtocol::SendTo( Ptr< Socket> socket, Ptr< Packet> packet, Ipv6Address destination){
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "send to ")<< destination);
      int ret= socket->SendTo( packet, 0, Inet6SocketAddress( destination, MCIH_PORT));
      if( ret< 0){
        NS_LOG_LOGIC( Utility::Coloring( RED, " * * * * * * * * * * * * * * "));
        NS_LOG_LOGIC( Utility::Coloring( RED, "  E R R O   R O N   S E N D "));
        NS_LOG_LOGIC( Utility::Coloring( RED, " * * * * * * * * * * * * * * "));
      } else{
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "send packet is completed, and packet size is ")<< ret);
      }
    }

    void RoutingProtocol::DeferredRouteOutput( Ptr< const Packet> packet, const Ipv6Header &header, UnicastForwardCallback unicast_callback, ErrorCallback error_callback){
      NS_LOG_FUNCTION( this);
      NS_ASSERT( packet!= 0&& packet!= Ptr< Packet>());

      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }

    void RoutingProtocol::DoDispose(){
      NS_LOG_FUNCTION( this);
    }

    void RoutingProtocol::DoInitialize(){
      NS_LOG_FUNCTION( this);

      bool has_global= false;
      for( uint32_t if_index= 0; if_index< ipv6->GetNInterfaces(); if_index++){
        auto l3= ipv6->GetObject< Ipv6L3Protocol>();
        auto interface= l3->GetInterface( if_index);
        interface->SetForwarding( true);
        bool active_interface= interface_exclusions.find( if_index)== interface_exclusions.end();
        for( uint32_t  addr_index= 0; addr_index< ipv6->GetNAddresses( if_index); addr_index++){
          auto address= ipv6->GetAddress( if_index, addr_index);
          if( FindSocket( address)) continue;
          if( address.GetScope()== Ipv6InterfaceAddress::LINKLOCAL&& active_interface){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "create socket to ")<< address.GetAddress());
            auto tid= TypeId::LookupByName( "ns3::UdpSocketFactory");
            auto node= GetObject< Node>();
            auto socket= Socket::CreateSocket( node, tid);
            auto local= Inet6SocketAddress( address.GetAddress(), MCIH_PORT);
            auto ret= socket->Bind( local);
            NS_ASSERT_MSG( ret== 0, "bind unsuccessful");
            socket->BindToNetDevice( ipv6->GetNetDevice( if_index));
            socket->ShutdownRecv();
            socket->SetIpv6RecvHopLimit( true);
            socket_interfaces.insert( make_pair( socket, interface));
            socket_addresses.insert( make_pair( socket, address));
          } else if( address.GetScope()== Ipv6InterfaceAddress::GLOBAL){
            has_global= true;
          }
        }
      }

      if( !receive_socket){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "create receiving socket"));
        auto tid= TypeId::LookupByName( "ns3::UdpSocketFactory");
        auto node= GetObject< Node>();
        receive_socket= Socket::CreateSocket( node, tid);
        auto local= Inet6SocketAddress( Ipv6Address::GetAny(), MCIH_PORT);
        receive_socket->Bind( local);
        receive_socket->SetRecvCallback( MakeCallback( &RoutingProtocol::ReceiveCallback, this));
        receive_socket->SetIpv6RecvHopLimit( true);
        receive_socket->SetRecvPktInfo( true);
      }

      if( has_global){
        NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "global address is had, but not implementation yet"));
      }

      NS_LOG_LOGIC( Utility::Coloring( MAGENTA, "scheduling is not implementation yet"));

      Ipv6RoutingProtocol::DoInitialize();
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
          });
    }

    static void Print( LogLevel level, Color color, Vector vec){
      static char msg[ 127];
      sprintf( msg, "%7.2lf, %7.2lf, %7.2lf", vec.x, vec.y, vec.z);
      NS_LOG( level, Utility::Coloring( color, "Vector")<< ": "<< msg);
    }

  }
} 

