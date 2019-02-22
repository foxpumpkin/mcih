#include "ns3/log.h"

#include "mcih-neighbor.h"
#include "mcih-utility.h"

using namespace std;
namespace ns3{
  NS_LOG_COMPONENT_DEFINE ("McihNeighbors");
  namespace mcih{
    void Neighbors::Print(){
      for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
          NS_LOG_LOGIC( "IP:"<< factor.neighbor_address<<
            "Mac: "<< factor.hardware_address<< ", "<< 
            "Exp: "<< factor.expire_time<< ", "<< 
            "Pos: ("<< factor.position.x<< ", "<< factor.position.y<< "), "<< 
            "Vel: ("<< factor.velocity.x<< ", "<< factor.velocity.y<< "), "<< 
            "Stt: "<< factor.state<< ", "<< 
            "Cls: "<< (factor.close?"T":"F")
            );
          });
    }
    Neighbors::Neighbors( Time delay): neighbor_timer(Timer::CANCEL_ON_DESTROY){
      NS_LOG_FUNCTION( this);
      neighbor_timer.SetDelay( delay);
      neighbor_timer.SetFunction( &Neighbors::Purge, this);
      tx_error_callback= MakeCallback( &Neighbors::ProcessTxError, this);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "tx callback is set"));
    }
    Neighbors::~Neighbors(){
      NS_LOG_FUNCTION( Utility::Coloring( RED, "not implement yet"));
    }
    Time Neighbors::GetExpireTime( Ipv6Address addr){
      NS_LOG_FUNCTION( this<< addr);
      Purge();
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "got expire time")<< itr->expire_time- Simulator::Now());
          return ( itr->expire_time- Simulator::Now());
        }
      }
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "expire time is not found, and return 0"));
      return Seconds( 0);
    }
    bool Neighbors::IsNeighbor( Ipv6Address addr){
      NS_LOG_FUNCTION( this<< addr);
      Purge();
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "is in neighbor list"));
          return true;
        }
      }
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "is not in neighbor list"));
      return false;
    }
    void Neighbors::SetCallback( Callback<void, Ipv6Address> cb){
      NS_LOG_FUNCTION( this);
      handle_link_failure = cb;
    }
    void Neighbors::ProcessTxError( WifiMacHeader const &hdr){
      NS_LOG_FUNCTION(this);
      Mac48Address addr = hdr.GetAddr1();
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->hardware_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "tx error node is found"));
          itr->close= true;
        }
      }
      Purge ();
    }


    void Neighbors::Purge(){
      // NS_LOG_FUNCTION( this);
      if( neighbor.empty()){
        NS_LOG_LOGIC( Utility::Coloring( CYAN, "neighbor list is empty"));
        return;
      }

      CloseNeighbor pred;
      if( !handle_link_failure.IsNull()){
        NS_LOG_LOGIC("handle link failer is not null");
        for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
          if( pred( *itr)){
            NS_LOG_LOGIC("close link to "<< itr->neighbor_address);
            handle_link_failure( itr->neighbor_address);
          }
        }
      }

      // NS_LOG_LOGIC( Utility::Coloring( CYAN, "delete close neighbor"));
      neighbor.erase( remove_if( neighbor.begin(), neighbor.end(), pred), neighbor.end());
      neighbor_timer.Cancel();
      neighbor_timer.Schedule();
    }

    void Neighbors::ScheduleTimer(){
      NS_LOG_FUNCTION( this);
      neighbor_timer.Cancel();
      neighbor_timer.Schedule();
    }

    void Neighbors::AddNdiscCache( Ptr< NdiscCache> ndisc){
      NS_LOG_FUNCTION( this);
      ndisc_vector.push_back( ndisc);
    }

    void Neighbors::DelNdiscCache(Ptr< NdiscCache> ndisc){
      NS_LOG_FUNCTION( this);
      ndisc_vector.erase( remove( ndisc_vector.begin(), ndisc_vector.end(), ndisc), ndisc_vector.end());
    }

    Mac48Address Neighbors::LookupMacAddress( Ipv6Address addr){
      NS_LOG_FUNCTION( this);
      Mac48Address hwaddr;
      for( auto itr= ndisc_vector.begin(); itr!= ndisc_vector.end(); ++itr){
        auto *entry= ( *itr)->Lookup( addr);
        if( entry){// != 0 && entry->IsAlive () && !entry->IsExpired ())
          if( !entry->IsIncomplete()){
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "ndisc entry is found")<< entry);
            //NS_LOG_LOGIC( "Delay "<< ( entry->IsDelay()?"T":"F"));
            //NS_LOG_LOGIC( "Incomplete "<< ( entry->IsIncomplete()?"T":"F"));
            //NS_LOG_LOGIC( "Probe "<< ( entry->IsProbe()?"T":"F"));
            //NS_LOG_LOGIC( "Reachable "<< ( entry->IsReachable()?"T":"F"));
            //NS_LOG_LOGIC( "Router "<< ( entry->IsRouter()?"T":"F"));
            //NS_LOG_LOGIC( "Stale "<< ( entry->IsStale()?"T":"F"));
            hwaddr= Mac48Address::ConvertFrom( entry->GetMacAddress());
            NS_LOG_LOGIC( Utility::Coloring( CYAN, "ndisc entry is found"));
            break;
          }
        }
      }
      return hwaddr;
    }

    Ipv6Address Neighbors::GetHighestRpmNeighborAddress(){
      Purge();
      if( !neighbor.size()){
        NS_LOG_FUNCTION( Utility::Coloring( RED, "neighbor list is empty"));
        return Ipv6Address();
      }
      auto max_itr= neighbor.begin();
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        if( max_itr->rpm< itr->rpm){
          max_itr= itr;
        }
      }
      return max_itr->neighbor_address;
    }

    Ipv6Address Neighbors::GetLowestRpmNeighborAddress(){
      // NS_LOG_FUNCTION( this);
      Purge();
      if( !neighbor.size()){
        NS_LOG_FUNCTION( Utility::Coloring( RED, "neighbor list is empty"));
        return Ipv6Address();
      }
      auto min_itr= neighbor.begin();
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        if( min_itr->rpm> itr->rpm){
          min_itr= itr;
        }
      }
      return min_itr->neighbor_address;
    }

    bool Neighbors::DelEntry( Ipv6Address addr){
      NS_LOG_FUNCTION( this<< addr);
      Purge();
      auto itr= find( neighbor.begin(), neighbor.end(), addr);
      if( itr== neighbor.end()){
        NS_LOG_LOGIC( "target entry is not found in list");
        return false;
      }
      neighbor.erase(itr);
      Purge();
      return true;
    }

    double NeighborNodes::GetRelativePositionAndMobility( double alpha, Vector position, Vector velocity) const{
      if( !neighbor.size()) return 1;
      Vector center_position( GetCenterPosition( position));
      vector< double> scalar_velocity( GetScalarVelocity( velocity));

      // velocity vector and center position
      for_each( neighbor.begin(), neighbor.end(), [ &]( auto factor){
          NS_LOG_LOGIC( string( Utility::Coloring( CYAN, "position " ))<< factor.position.x<< ", "<< factor.position.y<< ", "<< factor.position.z<< ", "
            << string( Utility::Coloring( CYAN, "velocity "))<< factor.velocity.x<< ", "<< factor.velocity.y<< ", "<< factor.velocity.z<< " -> "<< GetScalar( factor.velocity)<< ", "
            << string( Utility::Coloring( CYAN, "address "))<< factor.neighbor_address<< ", "
            << string( Utility::Coloring( CYAN, "rpm "))<< factor.rpm<< ", "
            << string( Utility::Coloring( CYAN, "time "))<< ( factor.expire_time- Simulator::Now()).As( Time::Unit::MS));
          });
      sort( scalar_velocity.begin(), scalar_velocity.end());
      double median_velocity= GetMedian( scalar_velocity);

      vector< double> relative_distance;
      vector< double> relative_speed;
      relative_distance.push_back( GetScalar( GetDistance( center_position, position)));
      relative_speed.push_back( abs( median_velocity- GetScalar( velocity)));
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        relative_distance.push_back( GetScalar( GetDistance( center_position, itr->position)));
        relative_speed.push_back( abs( median_velocity- GetScalar( itr->velocity)));
      }
      double max_relative_distance= *max_element( relative_distance.begin(), relative_distance.end());
      double max_relative_speed= *max_element( relative_speed.begin(), relative_speed.end());
      double relative_distance_1c= GetScalar( GetDistance( center_position, position));
      double relative_speed_1c= abs( median_velocity- GetScalar( velocity));

      RPM rpm= alpha*(relative_distance_1c/max_relative_distance)+ ( 1- alpha)* ( relative_speed_1c/ max_relative_speed);
      NS_LOG_LOGIC( Utility::Coloring( CYAN, "RPM") <<": "<< rpm<< "="<< alpha<< "("<< relative_distance_1c<< "/"<< max_relative_distance<< ")+(1-"<< alpha<< ")"<< relative_speed_1c<< "/"<< max_relative_speed);

      return rpm;
    }

    void NeighborNodes::Update( Ipv6Address addr, Time expire, UnadvHeader header){
      NS_LOG_FUNCTION( this<< addr<< expire);
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "target ip address is found, and updating expire timer"));
          itr->expire_time= std::max( expire+ Simulator::Now(), itr->expire_time);
          itr->position= header.GetPosition();
          itr->velocity= header.GetVelocity();
          itr->rpm= header.GetRelativePositionAndMobility();
          itr->role= Undecided;
          // if (itr->hardware_address== Mac48Address()){
          //   NS_LOG_LOGIC( Utility::Coloring( CYAN, "updating mac address is necessary"));
          //   itr->hardware_address= LookupMacAddress( itr->neighbor_address);
          // }
          return;
        }
      }
      NS_LOG_LOGIC( "open link to " << addr);
      Neighbor neighbor_instance( addr, LookupMacAddress( addr), expire+ Simulator::Now(), header.GetPosition(), header.GetVelocity(), header.GetRelativePositionAndMobility());
      neighbor.push_back( neighbor_instance);
      Purge();
    }

    void NeighborNodes::Update( Ipv6Address addr, Time expire, HelloHeader header){
      NS_LOG_FUNCTION( this<< addr<< expire);
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "target ip address is found, and updating expire timer"));
          itr->expire_time= std::max( expire+ Simulator::Now(), itr->expire_time);
          itr->position= header.GetPosition();
          itr->velocity= header.GetVelocity();
          itr->rpm= header.GetRelativePositionAndMobility();
          itr->rsm= header.GetRelativeStateAndMobility();
          itr->role= header.GetRole();
          // if (itr->hardware_address== Mac48Address()){
          //   NS_LOG_LOGIC( Utility::Coloring( CYAN, "updating mac address is necessary"));
          //   itr->hardware_address= LookupMacAddress( itr->neighbor_address);
          // }
          return;
        }
      }
      NS_LOG_LOGIC( "open link to " << addr);
      Neighbor neighbor_instance( addr, LookupMacAddress( addr), expire+ Simulator::Now(), header.GetPosition(), header.GetVelocity(), header.GetRelativePositionAndMobility(), header.GetRole());
      neighbor.push_back( neighbor_instance);
      Purge();
    }

    double NeighborHeaders::GetRelativeStateAndMobility( double alpha, State state, Vector velocity, uint32_t ch_index) const{
      if( !neighbor.size()) return 1;

      State best_state= GetBestState();
      if( !best_state) return 2;
      auto highest_rel_speed_vector= GetScalarRelativeVelocity( velocity);
      sort( highest_rel_speed_vector.begin(), highest_rel_speed_vector.end());
      auto highest_rel_speed= highest_rel_speed_vector[ highest_rel_speed_vector.size()- 1];
      double median_velocity= GetMedian( highest_rel_speed_vector);
      // NS_LOG_LOGIC(  "MEDIAN  "<< median_velocity<< ", MAX "<< highest_rel_speed);

      auto header= neighbor[ch_index];
      double relative_speed_1c= abs( GetScalar( header.velocity)- GetScalar( velocity));

      RSM rsm= alpha* ( double)( best_state- state)/ ( double)best_state+ ( 1- alpha)* relative_speed_1c/ highest_rel_speed;
      // NS_LOG_LOGIC( "RSM: "<< rsm<< " = "<< ( double)state/ ( double)best_state<< " + "<< relative_speed_1c/ highest_rel_speed);
      // NS_LOG_LOGIC( state<< ", "<< best_state<< " - "<< relative_speed_1c<< ", "<< highest_rel_speed);
      return rsm;
    }

    void NeighborHeaders::Update( Ipv6Address addr, Time expire, MchadvHeader header, Vector now_position, Vector now_velocity){
      NS_LOG_FUNCTION( this<< addr<< expire);
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "target ip address is found, and updating expire timer"));
          itr->expire_time= std::max( expire+ Simulator::Now(), itr->expire_time);
          itr->position= header.GetPosition();
          itr->velocity= header.GetVelocity();
          // itr->rpm= header.GetRelativePositionAndMobility();
          Vector rel_pos= GetDistance( itr->position, now_position);
          Vector rel_vel= GetDistance( itr->velocity, now_velocity);
          // NS_LOG_LOGIC( "TEST "<< rel_pos.x<< ", "<< rel_pos.y);
          // NS_LOG_LOGIC( "TEST "<< rel_vel.x<< ", "<< rel_vel.y);
          itr->state= CalcState( rel_pos, rel_vel);
          itr->rsm= GetRelativeStateAndMobility( 0.5, itr->state, now_velocity, distance( neighbor.begin(), itr));
          // NS_LOG_LOGIC( "STATE "<< ToString( itr->state));
          return;
        }

        for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
          itr->rsm= GetRelativeStateAndMobility( 0.5, itr->state, now_velocity, distance( neighbor.begin(), itr));
        }
      }
      NS_LOG_LOGIC( "open link to " << addr);
      Neighbor neighbor_instance( addr, LookupMacAddress( addr), expire+ Simulator::Now(), header.GetPosition(), header.GetVelocity(), header.GetRelativePositionAndMobility());
      neighbor.push_back( neighbor_instance);
      Purge();
    }

    void NeighborHeaders::Update( Ipv6Address addr, Time expire, HelloHeader header, Vector now_position, Vector now_velocity){
      NS_LOG_FUNCTION( this<< addr<< expire);
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "target ip address is found, and updating expire timer"));
          itr->expire_time= std::max( expire+ Simulator::Now(), itr->expire_time);
          itr->position= header.GetPosition();
          itr->velocity= header.GetVelocity();
          Vector rel_pos= GetDistance( itr->position, now_position);
          Vector rel_vel= GetDistance( itr->velocity, now_velocity);
          itr->state= CalcState( rel_pos, rel_vel);
          itr->rsm= GetRelativeStateAndMobility( 0.5, itr->state, now_velocity, distance( neighbor.begin(), itr));
          return;
        }

        for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
          itr->rsm= GetRelativeStateAndMobility( 0.5, itr->state, now_velocity, distance( neighbor.begin(), itr));
        }
      }
      NS_LOG_LOGIC( "open link to " << addr);
      Neighbor neighbor_instance( addr, LookupMacAddress( addr), expire+ Simulator::Now(), header.GetPosition(), header.GetVelocity(), header.GetRelativePositionAndMobility());
      neighbor.push_back( neighbor_instance);
      Purge();
    }

    bool NeighborHeaders::SetOwnClusterHead( Ipv6Address address){
      auto itr= find( neighbor.begin(), neighbor.end(), Neighbor( address, Mac48Address(), Time()));
      if( itr== neighbor.end()){
        NS_LOG_FUNCTION( "unknown address"<< address);
        return false;
        //NS_ABORT_MSG( "unknown address");
      }

      own_cluster_head= *itr;
      return true;
    }

    Neighbors::Neighbor NeighborHeaders::GetBestHeader(){
      Neighbor best= own_cluster_head;
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); itr++){
        if( best.rsm> itr->rsm) best= *itr;
      }
      return best;
    }

    NS_LOG_COMPONENT_DEFINE ("ClusterMembers");
    void ClusterMembers::Update( Ipv6Address addr, Time expire){
      NS_LOG_FUNCTION( this<< "new entry"<< addr<< expire);
      NS_LOG_LOGIC( "open link to " << addr);
      Neighbor neighbor_instance( addr, LookupMacAddress( addr), expire+ Simulator::Now());
      neighbor.push_back( neighbor_instance);
      Purge();
    }

    void ClusterMembers::Update( Ipv6Address addr, Time expire, HelloHeader header){
      NS_LOG_FUNCTION( this<< addr<< expire);
      for( auto itr= neighbor.begin(); itr!= neighbor.end(); ++itr){
        if( itr->neighbor_address== addr){
          NS_LOG_LOGIC( Utility::Coloring( CYAN, "target ip address is found, and updating expire timer"));
          itr->expire_time= std::max( expire+ Simulator::Now(), itr->expire_time);
          itr->position= header.GetPosition();
          itr->velocity= header.GetVelocity();
          itr->rpm= header.GetRelativePositionAndMobility();
          itr->role= header.GetRole();
          NS_LOG_LOGIC( "update entry " << addr);
          return;
        }
      }
      NS_LOG_LOGIC( "non member " << addr);
      Purge();
    }
  };
};
