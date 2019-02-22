#ifndef MCIH_UTILITY_
#define MCIH_UTILITY_

#define ESC_BLACK "\e[30m"
#define ESC_RED "\e[31m"
#define ESC_GREEN "\e[32m"
#define ESC_YELLOW "\e[33m"
#define ESC_BLUE "\e[34m"
#define ESC_MAGENTA "\e[35m"
#define ESC_CYAN "\e[36m"
#define ESC_WHITE "\e[37m"
#define ESC_CLEAR "\e[m"

#include <string>
#include <sstream>
#include <random>

#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/ipv6-interface-address.h"
#include "ns3/ipv6-interface.h"
#include "ns3/vector.h"

enum Color{ BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE, CLEAR};
namespace ns3{
  namespace mcih{
    enum Role{
      Undecided= 0,
      ClusterMember= 1,
      SubClusterHead= 2,
      MasterClusterHead= 3,
      RoleNumber= 4
    };
    static std::random_device seed_gen;
    static std::default_random_engine engine(seed_gen());
    static std::uniform_real_distribution<> dist(0.0, 1.0);
    static const int16_t DIMENSION= 2;
    typedef double RPM;
    typedef double RSM;
    class Utility{
      public:
        template<typename Type> static const char* Coloring( Color c, Type t){
          static char msg[ 100];
          std::string color;
          switch( c){
            case BLACK: color= ESC_BLACK; break;
            case RED: color= ESC_RED; break;
            case GREEN: color= ESC_GREEN; break;
            case YELLOW: color= ESC_YELLOW; break;
            case BLUE: color= ESC_BLUE; break;
            case MAGENTA: color= ESC_MAGENTA; break;
            case CYAN: color= ESC_CYAN; break;
            case WHITE: color= ESC_WHITE; break;
            case CLEAR: color= ESC_CLEAR; break;
          };
          std::stringstream ss;
          ss<< t;
          sprintf( msg, "%s%s%s", color.c_str(), ss.str().c_str(), ESC_CLEAR);

          return msg;
        }

        static std::string Scope( Ipv6InterfaceAddress::Scope_e state){
          static char msg[ 100];
          static std::string ret;
          switch( state){
            case Ipv6InterfaceAddress::HOST: ret= "HOST"; break;
            case Ipv6InterfaceAddress::LINKLOCAL: ret= "LINKLOCAL"; break;
            case Ipv6InterfaceAddress::GLOBAL: ret= "GLOBAL"; break; 
          }
          sprintf( msg, "%s", ret.c_str());
          return msg;
        }

        static std::string State( Ipv6InterfaceAddress::State_e state){
          static char msg[ 100];
          static std::string ret;
          switch( state){
            case Ipv6InterfaceAddress::TENTATIVE: ret= "tentative"; break;
            case Ipv6InterfaceAddress::DEPRECATED: ret= "deprecated"; break;
            case Ipv6InterfaceAddress::PREFERRED: ret= "preferred"; break;
            case Ipv6InterfaceAddress::PERMANENT: ret= "permanent"; break;
            case Ipv6InterfaceAddress::HOMEADDRESS: ret= "homeaddress"; break;
            case Ipv6InterfaceAddress::TENTATIVE_OPTIMISTIC: ret= "tentative optimistic"; break;
            case Ipv6InterfaceAddress::INVALID: ret= "invalid"; break; 
          }
          sprintf( msg, "%s", ret.c_str());
          return msg;
        }

        static std::string InterfaceAddress( Ipv6InterfaceAddress interface){
          static char msg[ 100];
          std::stringstream stream;
          stream<< "Address: "<< interface.GetAddress()<< ", ";
          stream<< "Scope: "<< Scope( interface.GetScope())<< ", ";
          stream<< "State: "<< State( interface.GetState());
          sprintf( msg, "%s", stream.str().c_str());
          return msg;
        }

      private:
    };

    Vector GetDistance( Vector a, Vector b);
    double GetEuclidDistance( Vector a, Vector b);
    template< typename T> T GetMedian( std::vector< T> vec){
      T t;
      if( !vec.size()) return t;
      int index= vec.size()/ 2;
      if( vec.size()% 2){ // #odd
        t= vec[ index];
      } else{ // #even
        t= ( vec[ index- 1]+ vec[ index]) /2;
      }
      return t;
    }
    double GetScalar( Vector v);
    inline std::string ToString( Role r){
      switch( r){
        // enum Role{ Undecided, ClusterMember, SubClusterHead, MasterClusterHead, RoleNumber };
        case Undecided: return "Undecided";
        case ClusterMember: return "ClusterMember";
        case SubClusterHead: return "SubClusterHead";
        case MasterClusterHead: return "MasterClusterHead";
        case RoleNumber: return "RoleNumber, maybe bug";
        default: return "TO STRING ERROR";
      }
    }
  }
}

#endif // MCIH_UTILITY_
