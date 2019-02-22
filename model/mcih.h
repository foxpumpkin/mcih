/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MCIH_H
#define MCIH_H

#include <map>
#include <memory>

#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/ipv6-interface-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/mobility-module.h"
#include "ns3/loopback-net-device.h"

#include "mcih-routing-table.h"
#include "mcih-utility.h"
#include "mcih-neighbor.h"
#include "mcih-packet.h"

namespace ns3{
  namespace mcih{
    class RoutingProtocol: public Ipv6RoutingProtocol{
      public: // constructor and destructor
        RoutingProtocol();
        virtual ~RoutingProtocol();

      public: // pure virtual function
        virtual void NotifyAddAddress( uint32_t if_num, Ipv6InterfaceAddress address);
        virtual void NotifyAddRoute( Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t if_num, Ipv6Address prefixToUse= Ipv6Address::GetZero());
        virtual void NotifyInterfaceDown( uint32_t if_num);
        virtual void NotifyInterfaceUp( uint32_t if_num);
        virtual void NotifyRemoveAddress( uint32_t if_num, Ipv6InterfaceAddress address);
        virtual void NotifyRemoveRoute (Ipv6Address destination, Ipv6Prefix mask, Ipv6Address next_hop, uint32_t if_num, Ipv6Address prefixToUse= Ipv6Address::GetZero());
        virtual void PrintRoutingTable( Ptr< OutputStreamWrapper> stream) const;
        virtual Ptr< Ipv6Route> RouteOutput( Ptr< Packet> packet, const Ipv6Header &header, Ptr< NetDevice> output_interface, Socket::SocketErrno &sockerr);
        virtual bool RouteInput( Ptr< const Packet> packet, const Ipv6Header &header, Ptr< const NetDevice> device, UnicastForwardCallback unicast_callback, MulticastForwardCallback multicast_callback, LocalDeliverCallback local_callback, ErrorCallback error_callback);
        virtual void SetIpv6( Ptr< Ipv6> arg_ipv6);

      public: // static public number
        static TypeId GetTypeId();
        static const uint32_t MCIH_PORT;

      private: // private member variable
        Ptr< Ipv6> ipv6;
        Ptr< Node> node;
        Ptr<NetDevice> loopback_device; 
        std::map< Ptr< Socket>, Ptr< Ipv6Interface> > socket_interfaces;
        std::map< Ptr< Socket>, Ptr< Ipv6Interface> > receive_socket_interfaces;
        //std::map< Ptr< Socket>, Ipv6InterfaceAddress> socket_addresses;
        Role role;
        Time hello_interval;
        Time active_route_timeout;
        Time active_neighbor_timeout;
        Time active_member_timeout;
        Time role_check_interval;
        Time velocity_check_interval;
        Time elect_mch_interval;
        Time contention_interval;
        Timer role_check_timer;
        Timer velocity_check_timer;
        Timer elect_mch_timer;
        Timer empty_check_timer;
        McihRoutingTable mcih_routing_table;
        NeighborNodes neighbor_nodes;
        NeighborHeaders neighbor_headers;
        std::unique_ptr< ClusterMembers> cluster_members;
        std::set< uint32_t> interface_exclusions;
        Ptr< UniformRandomVariable> uniform_random_variable;
        Vector position;
        Vector velocity;
        bool initialized;
        size_t unbound;
        Role default_role;

        std::vector< Time> connectable;
        Time become_connectable_time;

      public: // public function
        int64_t AssignStreams( int64_t stream);
        void SendHello( Ipv6Address destination= Ipv6Address::GetAllRoutersMulticast());
        void SendUnadv( Ipv6Address destination);
        void SendElectMch( Ipv6Address destination);
        void SendMchadv( Ipv6Address destination);
        void SendRgstreq( Ipv6Address destination);
        void SendRgstrep( Ipv6Address destination, Ipv6Address target);
        void SendHandover( Ipv6Address destination);
        void SendResign( Ipv6Address destination);
        void SetRole( Role r);
        void SetDefaultRole( Role r);

      private: // private function
        void Start();
        // Ptr< Socket> FindSocket( Ipv6InterfaceAddress address) const;
        Ptr< Socket> FindSocket( Ptr< Ipv6Interface> interface) const;
        Ptr< Socket> FindReceiveSocket( Ptr< Ipv6Interface> interface) const;
        Ptr< Ipv6Interface> FindInterface( Ptr< Socket> socket) const;
        Ptr< Ipv6Interface> FindReceiveInterface( Ptr< Socket> socket) const;
        void ReceiveCallback( Ptr< Socket> socket);
        void ReceiveHello( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveUnadv( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveElectMch( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveMchadv( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveRgstreq( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveRgstrep( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        void ReceiveResign( Ptr< Packet> packet, Ipv6Address source, Ptr< Ipv6Interface> interface, uint8_t hoplimit);
        Ptr< Ipv6Route> LoopbackRoute( const Ipv6Header &header, Ptr< NetDevice> output_interface) const;
        void RoleCheckTimerExpire();
        void VelocityCheckTimerExpire();
        void ElectMchTimerExpire();
        void EmptyCheckTimerExpire();
        void SendTo( Ptr< Socket> socket, Ptr< Packet> packet, Ipv6Address destination);
        void AddNetworkRouteTo( Ipv6Address network_address, Ipv6Prefix network_prefix, uint32_t if_index); 
        void SendTriggeredRouteUpdate();
        void DoSendRouteUpdate( bool periodic);
        Ptr< Ipv6Interface> GetInterface( uint32_t if_index){
          return ipv6->GetObject< Ipv6L3Protocol>()->GetInterface( if_index);
        }
        Ptr< MobilityModel> GetMobilityModel(){ return ipv6? ipv6->GetObject<MobilityModel>(): 0;}
        // Vector GetDistance( Vector a, Vector b){ return Vector( a.x- b.x, a.y- b.y, a.z- b.z); }
        RPM GetRPM(){ return neighbor_nodes.GetRelativePositionAndMobility( 0.5, position, velocity);}
        bool HasLinkLocal( Ptr< Ipv6Interface> interface){
          for( int ad_index= 0; ad_index< interface->GetNAddresses(); ad_index++){
            if( interface->GetAddress( ad_index).GetScope()== Ipv6InterfaceAddress::LINKLOCAL) return true;
          }
          return false;
        }

      protected: // protected function
        virtual void DoDispose();
        void DoInitialize();
        bool IsOwnAddress( Ipv6Address address);
        Ipv6Address GetAddress( size_t if_index, Ipv6InterfaceAddress::Scope_e scope){
          auto l3= ipv6->GetObject< Ipv6L3Protocol>();
          auto interface= l3->GetInterface( if_index);
          for( size_t ad_index= 0; ad_index< interface->GetNAddresses(); ad_index++){
            auto address= interface->GetAddress( ad_index);
            if( address.GetScope()== scope) return address.GetAddress();
          }
          NS_ABORT_MSG( "NO ADDRESS");
        }
        Ptr< Ipv6Interface> ToInterface( uint32_t index){
          auto l3= ipv6->GetObject< Ipv6L3Protocol>();
          auto interface= l3->GetInterface( index);
          return interface;
        }
        uint32_t ToIndex( Ptr< Ipv6Interface> interface){
          auto l3= ipv6->GetObject< Ipv6L3Protocol>();
          for( uint32_t if_num= 0; if_num< ipv6->GetNInterfaces(); if_num++){
            if( interface== l3->GetInterface( if_num)) return if_num;
          }
          NS_ABORT_MSG( "no such interface");
        }
        void InterclusterHandover();
        void EmptyCheckUpdate( Time time);
        void ElectMchUpdate( Time time);
    };
    static Ptr< Ipv6> ipv6;
    static void Print( LogLevel level, Color color, Ipv6Address address);
    static void Print( LogLevel level, Color color, Ipv6InterfaceAddress address);
    static void Print( LogLevel level, Color color, Ptr< Ipv6Interface> interface);
    static void Print( LogLevel level, Color color, Ptr< Socket> socket);
    static void Print( LogLevel level, Color color, std::vector< Ptr< NdiscCache> > cache);
    static void Print( LogLevel level, Color color, Vector vec);
    static void Print( LogLevel level, Color color, Ptr< Ipv6Route> route);
  }
}

#endif /* MCIH_H */

