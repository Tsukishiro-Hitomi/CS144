#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point + static_cast<uint32_t> (n); // note that the + has been overloaded in "wrapping_integars.hh"
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t check_32 = static_cast<uint32_t>(checkpoint);
  uint32_t expected = this->raw_value_ - zero_point.raw_value_;
  uint32_t dist = expected - check_32;
  int32_t diff = static_cast<int32_t>(dist);
  
  if (diff < 0 && checkpoint < (0U - static_cast<uint32_t>(diff))) {
    return checkpoint + dist;
  }
  return checkpoint + diff;
}
