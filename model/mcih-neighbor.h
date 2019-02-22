#ifndef __MCIH_NEIGHBOR_H_
#define __MCIH_NEIGHBOR_H_

#include "ns3/simulator.h"
#include "ns3/timer.h"
#include "ns3/ipv4-address.h"
#include "ns3/callback.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/arp-cache.h"
#include "ns3/ndisc-cache.h"

#include "mcih-utility.h"
#include "mcih-packet.h"

namespace ns3{
  namespace mcih{
    class RoutingProtocol;
    class Neighbors{
      public:
        enum State{ Far= 0, Depart= 1, Approach= 2, Near= 3};
        std::string ToString( State state){
          switch( state){
            case Near:
              return "Near";
            case Approach:
              return "Approach";
            case Depart:
              return "Depart";
            case Far:
              return "Far";
            default:
              NS_ABORT_MSG( "invalid state");
          }
          return "STATE";
        }
        struct Neighbor {
          Ipv6Address neighbor_address;
          Mac48Address hardware_address;
          Time expire_time;
          Vector position;
          Vector velocity;
          RPM rpm;
          RSM rsm;
          State state;
          Role role;
          bool close;

          Neighbor( Ipv6Address ip, Mac48Address mac, Time t, Vector p, Vector v, RPM r, Role role= Undecided): neighbor_address( ip), hardware_address( mac), expire_time( t), position( p), velocity( v), rpm( r), role( role), close( false){
          }
          Neighbor( Ipv6Address ip, Mac48Address mac, Time t): neighbor_address( ip), hardware_address( mac), expire_time( t), position( Vector( 0, 0, 0)), velocity( Vector( 0, 0, 0)), rpm( 1), close( false){
          }
          bool operator== ( const Neighbor &target ) const{ return target.neighbor_address== neighbor_address; }
          bool operator== ( const Ipv6Address &target ) const{ return target== neighbor_address; }
        };
        void Print();
        struct CloseNeighbor {
          bool operator() (const Neighbors::Neighbor & nb) const {
            return ((nb.expire_time < Simulator::Now ()) || nb.close);
          }
        };

        Neighbors( Time delay);
        virtual ~Neighbors()= 0;
        Time GetExpireTime( Ipv6Address addr);
        bool IsNeighbor( Ipv6Address addr);
        void Purge();
        void ScheduleTimer();
        void Clear(){ neighbor.clear(); }
        void AddNdiscCache( Ptr< NdiscCache> ndisc);
        void DelNdiscCache( Ptr< NdiscCache> ndisc);
        std::vector< Ptr< NdiscCache> > GetNdiscCache() const{ return ndisc_vector;}
        Callback<void, WifiMacHeader const &> GetTxErrorCallback () const { return tx_error_callback; }
        void SetCallback( Callback<void, Ipv6Address> cb);
        Callback< void, Ipv6Address> GetCallBack() const{ return handle_link_failure;}
        size_t GetNeighborNumber(){ Purge(); return neighbor.size();};
        Mac48Address LookupMacAddress( Ipv6Address);
        Ipv6Address GetHighestRpmNeighborAddress();
        Ipv6Address GetLowestRpmNeighborAddress();
        bool DelEntry( Ipv6Address addr);
      private:
        Callback<void, Ipv6Address> handle_link_failure;
        Callback<void, WifiMacHeader const &> tx_error_callback;
        Timer neighbor_timer;
        std::vector< Ptr< NdiscCache> > ndisc_vector;
        void ProcessTxError( WifiMacHeader const &);
      protected:
        std::vector< Neighbor> neighbor;
        State CalcState( Vector rel_pos, Vector rel_vel){
          auto rel_distance= GetEuclidDistance( rel_pos, rel_vel);
          auto rel_pos_scalar= GetScalar( rel_pos);
          if( rel_distance< 50.0) return Near;
          if( rel_distance> 100.0) return Far;
          if( rel_distance> rel_pos_scalar){ // depart
            return Depart;
          } else{ // approach
            return Approach;
          }
        }
        Vector GetCenterPosition( Vector position) const{
          Vector center_position( 0, 0, 0);
          center_position.x+= position.x;
          center_position.y+= position.y;
          center_position.z+= position.z;
          for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
              center_position.x+= factor.position.x;
              center_position.y+= factor.position.y;
              center_position.z+= factor.position.z;
              });
          center_position.x/= neighbor.size()+ 1;
          center_position.y/= neighbor.size()+ 1;
          center_position.z/= neighbor.size()+ 1;
          return center_position;
        }
        std::vector< double> GetScalarVelocity( Vector velocity) const{
          std::vector< double> scalar_velocity;
          scalar_velocity.push_back( GetScalar( velocity));
          for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
              scalar_velocity.push_back( GetScalar( factor.velocity));
              });
          return scalar_velocity;
        }
        std::vector< double> GetScalarRelativeVelocity( Vector velocity) const{
          std::vector< double> scalar_velocity;
          Vector rel_vel( 0, 0, 0);
          for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
              rel_vel= GetDistance( factor.velocity, velocity);
              scalar_velocity.push_back( GetScalar( rel_vel));
              });
          return scalar_velocity;
        }
        State GetBestState() const{
          State state= Far;
          for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
              if( state< factor.state) state= factor.state;
              });
          return state;
        }
    };
    class NeighborNodes: public Neighbors{
      public:
        NeighborNodes( Time delay): Neighbors( delay){
        }
        virtual ~NeighborNodes(){
        }
        double GetRelativePositionAndMobility( double alpha, Vector position, Vector velocity) const;
        void Update( Ipv6Address addr, Time expire, UnadvHeader header);
        void Update( Ipv6Address addr, Time expire, HelloHeader header);
    };
    class NeighborHeaders: public Neighbors{
      public:
        NeighborHeaders( Time delay): Neighbors( delay), own_cluster_head( Ipv6Address(), Mac48Address(), Time()){
        }
        virtual ~NeighborHeaders(){
        }
        double GetRelativeStateAndMobility( double alpha, State state, Vector velocity, uint32_t ch_index) const;
        void Update( Ipv6Address addr, Time expire, MchadvHeader header, Vector now_position, Vector now_velocity);
        void Update( Ipv6Address addr, Time expire, HelloHeader header, Vector now_position, Vector now_velocity);
        bool SetOwnClusterHead( Ipv6Address address);
        Neighbor GetOwnClusterHead(){ return own_cluster_head; }
        Neighbor GetBestHeader();
        bool IsOwnClusterHead( Ipv6Address address){ return address== own_cluster_head.neighbor_address;}
      protected:
        State state;
        Neighbor own_cluster_head;
    };
    class ClusterMembers: public Neighbors{
      public:
        ClusterMembers( Time delay): Neighbors( delay){
        }
        virtual ~ClusterMembers(){
        }
        void Update( Ipv6Address addr, Time expire);
        void Update( Ipv6Address addr, Time expire, HelloHeader header);
    };
  }
}

#endif // __MCIH_NEIGHBOR_H_
