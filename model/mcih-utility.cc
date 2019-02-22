#include <string>
#include <sstream>

#include "mcih-utility.h"

using namespace std;

namespace ns3{
  NS_LOG_COMPONENT_DEFINE( "McihUtility");
  namespace mcih{
    Vector GetDistance( Vector a, Vector b){ return Vector( a.x- b.x, a.y- b.y, a.z- b.z); }
    double GetEuclidDistance( Vector a, Vector b){ return sqrt( pow( a.x- b.x, 2)+ pow( a.y- b.y, 2)+ pow( a.z- b.z, 2));}
    double GetScalar( Vector v){ return GetEuclidDistance( v, Vector( 0, 0, 0));}
  }
}
