#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  route_table_.push_back(route_entry(route_prefix, prefix_length, next_hop, interface_num));
}


// Go through all route entrys, find the entry with longest matched prefix (if not found, return -1)
int Router::find_longest_prefix_matched(uint32_t dst_ip_address) const {
  int result_index = -1;
  int longest_prefix_matched = -1;
  for(size_t i = 0; i < route_table_.size(); i++) {
    route_entry cur_entry = route_table_[i];

    uint32_t route_prefix = cur_entry.route_prefix;
    uint8_t prefix_length = cur_entry.prefix_length;
    
    // notice that for an uint32_t number: << 32 is an illegal operation
    uint32_t mask = (prefix_length == 0) ? 0 : (0xFFFFFFFF << (32 - prefix_length));
    bool is_matched = (dst_ip_address & mask) == (route_prefix & mask);

    if (is_matched && prefix_length > longest_prefix_matched) {
      longest_prefix_matched = prefix_length;
      result_index = i;
    }
  }
  return result_index;
};

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  // iterate all datagrams received in all interfaces
  for (auto& interface_ptr : _interfaces) {
    auto& datagram_qu = interface_ptr->datagrams_received();
    while (!datagram_qu.empty()) {
      InternetDatagram cur_datagram = datagram_qu.front();
      datagram_qu.pop();
      
      IPv4Header& header = cur_datagram.header;

      // drop the expired datagram
      if (header.ttl <= 1) {
        continue;
      }

      // decrease the ip_header ttl
      header.ttl -= 1;

      // remember to recompute the checksum since ttl is modified
      header.compute_checksum();

      uint32_t dst_ip_address = header.dst;

      // find the matched route entry
      int route_entry_index = find_longest_prefix_matched(dst_ip_address);

      // no matched entry found
      if (route_entry_index == -1) {
        continue;
      }

      route_entry matched_entry = route_table_[route_entry_index];
      optional<Address> next_hop = matched_entry.next_hop;
      size_t interface_num = matched_entry.interface_num;

      // if next_hop is empty: next_hop_address is it's ip_dst_address
      Address next_hop_address = next_hop.value_or(Address::from_ipv4_numeric(dst_ip_address));

      _interfaces.at(interface_num)->send_datagram(cur_datagram, next_hop_address);
    }
  }
}
