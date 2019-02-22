#ifndef __MCIH_PACKET_H_
#define __MCIH_PACKET_H_

// c libraries
#include <net/ethernet.h>
#include <netinet/in.h>
//#include <netinet6/in6.h>
#include <string.h>

// cxx libraries
#include <iostream>
#include <vector>

// ns3 libraries
#include "ns3/buffer.h"
#include "ns3/chunk.h"
#include "ns3/header.h"
#include "ns3/enum.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/mcih-utility.h"
#include "ns3/vector.h"
#include "ns3/abort.h"

namespace ns3{
   namespace mcih{
     typedef std::vector< uint16_t> Position;
     typedef std::vector< uint16_t> Velocity;

      enum MessageType {
         MCIHTYPE_HELLO= 0,
         MCIHTYPE_MCHADV= 1,
         MCIHTYPE_SCHADV= 2,
         MCIHTYPE_CMADV= 3,
         MCIHTYPE_UNADV= 4,
         MCIHTYPE_ELECTMCH= 5,
         MCIHTYPE_RGSTREQ= 6,
         MCIHTYPE_RGSTREP= 7,
         MCIHTYPE_CHRESIGN= 8
      };

      /*
       * Mcih Typeを保存する．
       */
      class TypeHeader: public Header {
         public:
            TypeHeader( MessageType t= MCIHTYPE_HELLO);

            static TypeId GetTypeId();
            TypeId GetInstanceTypeId() const;
            uint32_t GetSerializedSize() const;
            void Serialize( Buffer::Iterator start) const;
            uint32_t Deserialize( Buffer::Iterator start);
            void Print( std::ostream &os) const;

            MessageType GetType() const{ return m_type;}
            bool IsValid() const{ return m_valid;}
            bool operator==( TypeHeader const & o) const;
         private:
            MessageType m_type;
            bool m_valid;
      };
      std::ostream &operator<<( std::ostream &os, TypeHeader const &h);


      /* テンプレート用のダミーヘッダ
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |     Type      |J|R|G|D|U|   Reserved          |   Hop Count   |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class HelloHeader: public Header{
         public:
            HelloHeader(); // ObjectBaseにデフォルト引数を要求されるので，デフォルト引数を与えなければならない．
            static TypeId GetTypeId();
            TypeId GetInstanceTypeId( void) const; // ns3::ObjectBase
            uint32_t GetSerializedSize() const;
            void Serialize( Buffer::Iterator start) const;
            uint32_t Deserialize( Buffer::Iterator start);
            void Print( std::ostream &os) const;

            bool IsValid() const{ return m_valid; }
            Ipv6Address GetAddress() const{ return address;}
            Vector GetPosition() const{ return position;}
            Vector GetVelocity() const{ return velocity;}
            RPM GetRelativePositionAndMobility(){ return rpm;}
            RSM GetRelativeStateAndMobility(){ return rsm;}
            Role GetRole(){ return role;}
            size_t GetAbsCm(){ return abs_cm;}
            void SetAddress( Ipv6Address addr){ address= addr;}
            void SetPosition( Vector pos){ position= pos;}
            void SetVelocity( Vector vel){ velocity= vel;}
            void SetRelativePositionAndMobility( RPM r){ rpm= r;}
            void SetRelativeStateAndMobility( RSM r){ rsm= r;}
            void SetRole( Role r){ role= r;}
            void SetAbsCm( size_t a){ abs_cm= a;}
            bool operator==( HelloHeader const & o) const;
         private:
            bool m_valid;
            Ipv6Address address;
            Vector position;
            Vector velocity;
            RPM rpm;
            RSM rsm;
            Role role;
            size_t abs_cm;
      };
      std::ostream & operator<< (std::ostream & os, HelloHeader const & h);

      /* Undecided Advertisement
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |     Type      |    Reserved   |        Sequence Number        |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |          Position_x           |          Position_y           |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |          Velocity_x           |          Velocity_y           |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class UnadvHeader: public Header{
        public:
          UnadvHeader();
          virtual ~UnadvHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( UnadvHeader const & o) const;
        private:
          uint8_t reserved;
          uint16_t sequence;
          Vector position;
          Vector velocity;
          RPM rpm;
        public: // accesser for parameter
          Vector GetPosition() const{ return position;}
          Vector GetVelocity() const{ return velocity;}
          RPM GetRelativePositionAndMobility(){ return rpm;}
          void SetPosition( Vector pos){ position= pos;}
          void SetVelocity( Vector vel){ velocity= vel;}
          void SetRelativePositionAndMobility( RPM r){ rpm= r;};
      };
      std::ostream & operator<<( std::ostream & os, UnadvHeader const & h);


      /* Master Cluster Head Advertisement
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |     Type      |    Reserved   |        Sequence Number        |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |          Position_x           |          Position_y           |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |          Velocity_x           |          Velocity_y           |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class MchadvHeader: public Header{
        public:
          MchadvHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( MchadvHeader const &o) const;
        private:
          Vector position;
          Vector velocity;
          RPM rpm;
          Ipv6Address mch_address;
        public: // accesser for parameter
          Vector GetPosition() const{ return position;}
          Vector GetVelocity() const{ return velocity;}
          RPM GetRelativePositionAndMobility(){ return rpm;}
          void SetPosition( Vector pos){ position= pos;}
          void SetVelocity( Vector vel){ velocity= vel;}
          void SetRelativePositionAndMobility( RPM r){ rpm= r;}
          Ipv6Address GetMchAddress(){ return mch_address;}
          void SetMchAddress( Ipv6Address address){ mch_address= address; }
      };
      std::ostream &operator<<( std::ostream & os, MchadvHeader const & h);

      /* ElectMaster Cluster Head
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |                                                               |
       * |                                                               |
       * |                              ipv6                             |
       * |                                                               |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class ElectMchHeader: public Header{
        public:
          ElectMchHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          Ipv6Address GetTargetAddress() const{ return elect_server_address;}
          void SetTargetAddress( const Ipv6Address dst){ elect_server_address= dst; }
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( ElectMchHeader const &o) const;
        private:
          Ipv6Address elect_server_address;
      };
      std::ostream &operator<<( std::ostream & os, ElectMchHeader const & h);

      /* RegistrationRequest Request
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |                                                               |
       * |                                                               |
       * |                              ipv6                             |
       * |                                                               |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class RgstreqHeader: public Header{
        public:
          RgstreqHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          Ipv6Address GetTargetAddress() const{ return router_address;}
          void SetTargetAddress( const Ipv6Address dst){ router_address= dst; }
          Ipv6Address GetRegistAddress() const{ return regist_address;}
          void SetRegistAddress( const Ipv6Address dst){ regist_address= dst; }
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( RgstreqHeader const &o) const;
        private:
          Ipv6Address router_address;
          Ipv6Address regist_address;
      };
      std::ostream &operator<<( std::ostream & os, RgstreqHeader const & h);

      /* RegistrationReply Request
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |                                                               |
       * |                                                               |
       * |                              ipv6                             |
       * |                                                               |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class RgstrepHeader: public Header{
        public:
          RgstrepHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          Ipv6Address GetHeaderAddress() const{ return router_address;}
          void SetHeaderAddress( const Ipv6Address dst){ router_address= dst; }
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( RgstrepHeader const &o) const;
        private:
          Ipv6Address router_address;
      };
      std::ostream &operator<<( std::ostream & os, RgstrepHeader const & h);

      /* CH Resign
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |                                                               |
       * |                                                               |
       * |                              ipv6                             |
       * |                                                               |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class ResignHeader: public Header{
        public:
          ResignHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          Ipv6Address GetHeaderAddress() const{ return address;}
          void SetHeaderAddress( const Ipv6Address dst){ address= dst; }
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( ResignHeader const &o) const;
        private:
          Ipv6Address address;
      };
      std::ostream &operator<<( std::ostream & os, ResignHeader const & h);

      /* ElectSub Cluster Head
       *  0                   1                   2                   3
       *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       * |                                                               |
       * |                                                               |
       * |                              ipv6                             |
       * |                                                               |
       * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       */
      class ElectSchHeader: public Header{
        public:
          ElectSchHeader();
          static TypeId GetTypeId();
          TypeId GetInstanceTypeId( void) const;
          Ipv6Address GetTargetAddress() const{ return elect_server_address;}
          void SetTargetAddress( const Ipv6Address dst){ elect_server_address= dst; }
          uint32_t GetSerializedSize() const;
          void Serialize( Buffer::Iterator start) const;
          uint32_t Deserialize( Buffer::Iterator start);
          void Print( std::ostream &os) const;

          bool operator==( ElectSchHeader const &o) const;
        private:
          Ipv6Address elect_server_address;
      };
      std::ostream &operator<<( std::ostream & os, ElectSchHeader const & h);


   }
}

#endif // __MCIH_PACKET_H_
