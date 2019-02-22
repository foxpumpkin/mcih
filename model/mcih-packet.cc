#include "mcih-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/header.h"
#include "ns3/log.h"

namespace ns3{
  namespace mcih{
    NS_LOG_COMPONENT_DEFINE ("McihPacket");
    NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

    TypeHeader::TypeHeader (MessageType t) : m_type (t), m_valid (true) {
    }
    TypeId TypeHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::TypeHeader").SetParent<Header> ().SetGroupName("Mcih").AddConstructor<TypeHeader> ();
      return tid;
    }
    TypeId TypeHeader::GetInstanceTypeId () const {
      return GetTypeId ();
    }
    uint32_t TypeHeader::GetSerializedSize () const {
      return 1;
    }
    void TypeHeader::Serialize (Buffer::Iterator i) const {
      i.WriteU8 ((uint8_t) m_type);
    }
    uint32_t TypeHeader::Deserialize (Buffer::Iterator start) {
      Buffer::Iterator i = start;
      uint8_t type = i.ReadU8 ();
      m_valid= true;
      switch( type) {
        case MCIHTYPE_HELLO:
        case MCIHTYPE_MCHADV:
        case MCIHTYPE_SCHADV:
        case MCIHTYPE_CMADV:
        case MCIHTYPE_UNADV:
        case MCIHTYPE_ELECTMCH:
        case MCIHTYPE_RGSTREQ:
        case MCIHTYPE_RGSTREP:
        case MCIHTYPE_CHRESIGN:
          m_type= ( MessageType) type;
          break;
        default:
          NS_ABORT_MSG("INVALID MESSAGE TYPE");
          m_valid = false;
      }
      // NS_ASSERT_MSG( m_valid== false, "invalid header type is set");
      uint32_t dist = i.GetDistanceFrom (start);
      NS_ASSERT (dist == GetSerializedSize ());
      return dist;
    }
    void TypeHeader::Print (std::ostream &os) const {
      switch (m_type) {
        case MCIHTYPE_HELLO:
          os<< "HELLO";
          break;
        case MCIHTYPE_MCHADV:
          os<< "MCHADV";
          break;
        case MCIHTYPE_SCHADV:
          os<< "SCHADV";
          break;
        case MCIHTYPE_CMADV:
          os<< "CMADV";
          break;
        case MCIHTYPE_UNADV:
          os<< "UNADV";
          break;
        case MCIHTYPE_ELECTMCH:
          os<< "ELECTMCH";
          break;
        case MCIHTYPE_RGSTREQ:
          os<< "RGSTREQ";
          break;
        case MCIHTYPE_RGSTREP:
        default:
          os<< "RGSTREP";
          break;
          os<< "UNKNOWN_TYPE";
      }
    }
    bool TypeHeader::operator== (TypeHeader const & o) const {
      return (m_type == o.m_type && m_valid == o.m_valid);
    }
    std::ostream & operator<< (std::ostream & os, TypeHeader const & h) {
      h.Print (os);
      return os;
    }

    // テンプレート用のダミーヘッダ
    NS_OBJECT_ENSURE_REGISTERED (HelloHeader);
    HelloHeader::HelloHeader(): m_valid (true){
    }
    TypeId HelloHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::HelloHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<HelloHeader> ();
      return tid;
    }
    TypeId HelloHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t HelloHeader::GetSerializedSize () const{
      return (128+ 64*4+64+64+32+32)/8;
    }
    void HelloHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, address);

      uint64_t px, py, vx, vy;
      memcpy( &px, &position.x, sizeof( position.x));
      memcpy( &py, &position.y, sizeof( position.y));
      memcpy( &vx, &velocity.x, sizeof( velocity.x));
      memcpy( &vy, &velocity.y, sizeof( velocity.y));
      start.WriteU64( px);
      start.WriteU64( py);
      start.WriteU64( vx);
      start.WriteU64( vy);

      uint64_t u64_rpm;
      memcpy( &u64_rpm, &rpm, sizeof( rpm));
      start.WriteU64( u64_rpm);
      uint64_t u64_rsm;
      memcpy( &u64_rsm, &rsm, sizeof( rsm));
      start.WriteU64( u64_rsm);

      // NS_LOG_LOGIC( "HELLO SERIALIZE "<< ToString( role)<< " "<< static_cast<uint32_t>(role));
      start.WriteU32( static_cast<uint32_t>(role));
      start.WriteU32( abs_cm);
    }
    uint32_t HelloHeader::Deserialize (Buffer::Iterator start){
      Buffer::Iterator i = start;

      ReadFrom( i, address);
      uint64_t px, py, vx, vy;
      px= i.ReadU64();
      py= i.ReadU64();
      vx= i.ReadU64();
      vy= i.ReadU64();
      memcpy( &position.x, &px, sizeof( velocity.x));
      memcpy( &position.y, &py, sizeof( velocity.y));
      memcpy( &velocity.x, &vx, sizeof( velocity.x));
      memcpy( &velocity.y, &vy, sizeof( velocity.y));

      uint64_t u64_rpm;
      u64_rpm= i.ReadU64();
      memcpy( &rpm, &u64_rpm, sizeof( rpm));
      uint64_t u64_rsm;
      u64_rsm= i.ReadU64();
      memcpy( &rsm, &u64_rsm, sizeof( rsm));

      uint32_t r= i.ReadU32();
      role= static_cast<Role>(r);
      abs_cm= i.ReadU32();
      // NS_LOG_LOGIC( "HELLO DESERIALIZE "<< ToString( static_cast<Role>( r))<< " "<< static_cast<Role>(r)<< " "<< rsm<< " "<< abs_cm);

      uint32_t dist= i.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void HelloHeader::Print (std::ostream &os) const{
      os << "HELLO";
    }
    bool HelloHeader::operator== (HelloHeader const & o) const {
      return true;
    }
    std::ostream & operator<< (std::ostream & os, HelloHeader const & h) {
      h.Print (os);
      return os;
    }

    // 
    NS_OBJECT_ENSURE_REGISTERED (UnadvHeader);
    UnadvHeader::UnadvHeader(): reserved( 0), sequence( 0){
      //position.resize( DIMENSION);
      //velocity.resize( DIMENSION);
    }

    UnadvHeader::~UnadvHeader(){
      NS_LOG_FUNCTION( this);
    }

    TypeId UnadvHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::UnadvHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<UnadvHeader> ();
      return tid;
    }
    TypeId UnadvHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t UnadvHeader::GetSerializedSize () const{
      return 1+ 2+ ( 8 + 8) * DIMENSION+ 8;
    }
    void UnadvHeader::Serialize (Buffer::Iterator start) const{
      start.WriteU8( reserved);
      start.WriteU16( sequence);

      uint64_t px, py, vx, vy;
      memcpy( &px, &position.x, sizeof( position.x));
      memcpy( &py, &position.y, sizeof( position.y));
      memcpy( &vx, &velocity.x, sizeof( velocity.x));
      memcpy( &vy, &velocity.y, sizeof( velocity.y));
      start.WriteU64( px);
      start.WriteU64( py);
      start.WriteU64( vx);
      start.WriteU64( vy);

      uint64_t u64_rpm;
      memcpy( &u64_rpm, &rpm, sizeof( rpm));
      start.WriteU64( u64_rpm);
    }
    uint32_t UnadvHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator i = start;
      reserved= i.ReadU8();
      sequence= i.ReadU16();

      uint64_t px, py, vx, vy;
      px= i.ReadU64();
      py= i.ReadU64();
      vx= i.ReadU64();
      vy= i.ReadU64();
      memcpy( &position.x, &px, sizeof( velocity.x));
      memcpy( &position.y, &py, sizeof( velocity.y));
      memcpy( &velocity.x, &vx, sizeof( velocity.x));
      memcpy( &velocity.y, &vy, sizeof( velocity.y));

      uint64_t u64_rpm;
      u64_rpm= i.ReadU64();
      memcpy( &rpm, &u64_rpm, sizeof( rpm));

      uint32_t dist= i.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void UnadvHeader::Print( std::ostream &os) const{
      NS_LOG_FUNCTION( this);
      os<< "Unadv";
    }
    bool UnadvHeader::operator== (UnadvHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, UnadvHeader const & h) {
      h.Print (os);
      return os;
    }

    // 
    NS_OBJECT_ENSURE_REGISTERED (MchadvHeader);
    MchadvHeader::MchadvHeader(){
    }
    TypeId MchadvHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::MchadvHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<MchadvHeader> ();
      return tid;
    }
    TypeId MchadvHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t MchadvHeader::GetSerializedSize () const{
      return 16+ ( 8 + 8) * DIMENSION+ 8;
    }
    void MchadvHeader::Serialize (Buffer::Iterator start) const{
      uint64_t px, py, vx, vy;
      memcpy( &px, &position.x, sizeof( position.x));
      memcpy( &py, &position.y, sizeof( position.y));
      memcpy( &vx, &velocity.x, sizeof( velocity.x));
      memcpy( &vy, &velocity.y, sizeof( velocity.y));
      start.WriteU64( px);
      start.WriteU64( py);
      start.WriteU64( vx);
      start.WriteU64( vy);

      uint64_t u64_rpm;
      memcpy( &u64_rpm, &rpm, sizeof( rpm));
      start.WriteU64( u64_rpm);
      WriteTo( start, mch_address);
    }
    uint32_t MchadvHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator i = start;
    
      uint64_t px, py, vx, vy;
      px= i.ReadU64();
      py= i.ReadU64();
      vx= i.ReadU64();
      vy= i.ReadU64();
      memcpy( &position.x, &px, sizeof( velocity.x));
      memcpy( &position.y, &py, sizeof( velocity.y));
      memcpy( &velocity.x, &vx, sizeof( velocity.x));
      memcpy( &velocity.y, &vy, sizeof( velocity.y));

      uint64_t u64_rpm;
      u64_rpm= i.ReadU64();
      memcpy( &rpm, &u64_rpm, sizeof( rpm));
      ReadFrom( i, mch_address);

      uint32_t dist= i.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void MchadvHeader::Print (std::ostream &os) const{
      os<< "Mchadv";
    }
    bool MchadvHeader::operator== (MchadvHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, MchadvHeader const & h) {
      h.Print (os);
      return os;
    }
    
    // 
    NS_OBJECT_ENSURE_REGISTERED( ElectMchHeader);
    ElectMchHeader::ElectMchHeader(){
    }
    TypeId ElectMchHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::ElectMchHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<ElectMchHeader> ();
      return tid;
    }
    TypeId ElectMchHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t ElectMchHeader::GetSerializedSize () const{
      return 128/ 8;
    }
    void ElectMchHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, elect_server_address);
    }
    uint32_t ElectMchHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator itr = start;
      ReadFrom( itr, elect_server_address);

      uint32_t dist= itr.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void ElectMchHeader::Print (std::ostream &os) const{
      os<< "Elect Master Cluster Header";
    }
    bool ElectMchHeader::operator== (ElectMchHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, ElectMchHeader const & h) {
      h.Print (os);
      return os;
    }

    // 
    NS_OBJECT_ENSURE_REGISTERED( RgstreqHeader);
    RgstreqHeader::RgstreqHeader(){
    }
    TypeId RgstreqHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::RgstreqHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<RgstreqHeader> ();
      return tid;
    }
    TypeId RgstreqHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t RgstreqHeader::GetSerializedSize () const{
      return 128/ 8* 2;
    }
    void RgstreqHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, router_address);
      WriteTo( start, regist_address);
    }
    uint32_t RgstreqHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator itr = start;
      ReadFrom( itr, router_address);
      ReadFrom( itr, regist_address);

      uint32_t dist= itr.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void RgstreqHeader::Print (std::ostream &os) const{
      os<< "Elect Master Cluster Header";
    }
    bool RgstreqHeader::operator== (RgstreqHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, RgstreqHeader const & h) {
      h.Print (os);
      return os;
    }

    // 
    NS_OBJECT_ENSURE_REGISTERED( RgstrepHeader);
    RgstrepHeader::RgstrepHeader(){
    }
    TypeId RgstrepHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::RgstrepHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<RgstrepHeader> ();
      return tid;
    }
    TypeId RgstrepHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t RgstrepHeader::GetSerializedSize () const{
      return 128/ 8;
    }
    void RgstrepHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, router_address);
    }
    uint32_t RgstrepHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator itr = start;
      ReadFrom( itr, router_address);

      uint32_t dist= itr.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void RgstrepHeader::Print (std::ostream &os) const{
      os<< "Elect Master Cluster Header";
    }
    bool RgstrepHeader::operator== (RgstrepHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, RgstrepHeader const & h) {
      h.Print (os);
      return os;
    }

    // 
    NS_OBJECT_ENSURE_REGISTERED( ResignHeader);
    ResignHeader::ResignHeader(){
    }
    TypeId ResignHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::ResignHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<ResignHeader> ();
      return tid;
    }
    TypeId ResignHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t ResignHeader::GetSerializedSize () const{
      return 128/ 8;
    }
    void ResignHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, address);
    }
    uint32_t ResignHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator itr = start;
      ReadFrom( itr, address);

      uint32_t dist= itr.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void ResignHeader::Print (std::ostream &os) const{
      os<< "Cluster Head Resign";
    }
    bool ResignHeader::operator== (ResignHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, ResignHeader const & h) {
      h.Print (os);
      return os;
    }
 
    // 
    NS_OBJECT_ENSURE_REGISTERED( ElectSchHeader);
    ElectSchHeader::ElectSchHeader(){
    }
    TypeId ElectSchHeader::GetTypeId () {
      static TypeId tid = TypeId ("ns3::mcih::ElectSchHeader")
        .SetParent<Header> ()
        .SetGroupName("Mcih")
        .AddConstructor<ElectSchHeader> ();
      return tid;
    }
    TypeId ElectSchHeader::GetInstanceTypeId (void) const{
      return GetTypeId();
    }
    uint32_t ElectSchHeader::GetSerializedSize () const{
      return 128/ 8;
    }
    void ElectSchHeader::Serialize (Buffer::Iterator start) const{
      WriteTo( start, elect_server_address);
    }
    uint32_t ElectSchHeader::Deserialize( Buffer::Iterator start){
      Buffer::Iterator itr = start;
      ReadFrom( itr, elect_server_address);

      uint32_t dist= itr.GetDistanceFrom( start);
      NS_ASSERT( dist== GetSerializedSize ());
      return dist;
    }
    void ElectSchHeader::Print (std::ostream &os) const{
      os<< "Elect Master Cluster Header";
    }
    bool ElectSchHeader::operator== (ElectSchHeader const & o) const {
      return 1;
    }
    std::ostream & operator<< (std::ostream & os, ElectSchHeader const & h) {
      h.Print (os);
      return os;
    }
  } 
}
