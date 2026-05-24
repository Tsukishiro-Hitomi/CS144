#include <iostream>
#include <optional>
#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  uint32_t target_ip_address = next_hop.ipv4_numeric();
  auto it = arp_cache_.find(target_ip_address);

  // construct the ethernetframe and transmit it normally
  if (it != arp_cache_.end()) {
    arp_entry_ e = it->second;
    EthernetAddress dst_e_address = e.e_address;

    // construct the ethernet header
    EthernetHeader eheader;
    eheader.src = ethernet_address_;
    eheader.dst = dst_e_address;
    eheader.type = EthernetHeader::TYPE_IPv4;

    // construct the ethernet frame
    EthernetFrame eframe;
    eframe.header = eheader;
    eframe.payload = serialize(dgram);

    transmit(eframe);
  } else {
    // push the dgram into queued_datagrams_
    queued_datagrams_[target_ip_address].push_back(dgram);

    // check if we have sent the arp request
    if(arp_sent_.find(target_ip_address) != arp_sent_.end()) {
      return;
    } else {
      // construct the ethernet header
      EthernetHeader eheader;
      eheader.src = ethernet_address_;
      eheader.dst = ETHERNET_BROADCAST;
      eheader.type = EthernetHeader::TYPE_ARP;
      
      // construct the arp message (the ethernet frame's payload)
      ARPMessage arp_message;
      arp_message.opcode = ARPMessage::OPCODE_REQUEST;
      arp_message.sender_ip_address = ip_address_.ipv4_numeric();
      arp_message.sender_ethernet_address = ethernet_address_;

      arp_message.target_ip_address = target_ip_address;
      arp_message.target_ethernet_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // set to all zeros according to convention
      
      // construct the ethernet frame
      EthernetFrame eframe;
      eframe.header = eheader;
      eframe.payload = serialize(arp_message);

      transmit(eframe);

      // update the arp_sent_
      arp_sent_[target_ip_address] = 5000;
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  EthernetHeader ethernet_header = frame.header;
  vector<string> payload = frame.payload;
  EthernetAddress ethernet_src_address = ethernet_header.src;
  optional<uint32_t> ip_src_address;

  // check the ethernet header's type:
  if (ethernet_header.type == EthernetHeader::TYPE_IPv4) {
    // check if it is sent to us
    if (ethernet_header.dst == ethernet_address_ || ethernet_header.dst == ETHERNET_BROADCAST) {
      Parser parser(payload);
      InternetDatagram datagram;
      datagram.parse(parser);
      if (parser.has_error()) {
        return;
      }
      
      // push the datagram into datagrams_received_
      datagrams_received_.push(datagram);

      IPv4Header datagram_header = datagram.header;
      ip_src_address = datagram_header.src;
    }
  } else if (ethernet_header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arpmessage;
    Parser parser(payload);
    arpmessage.parse(parser);
    if (parser.has_error()) {
      return;
    }

    ip_src_address = arpmessage.sender_ip_address;

    // if the arp message is broadcasting to look for our ethernet address, we need to reply
    if (arpmessage.opcode == ARPMessage::OPCODE_REQUEST && arpmessage.target_ip_address == ip_address_.ipv4_numeric()) {
      // construct the ethernet header
      EthernetHeader eheader;
      eheader.src = ethernet_address_;
      eheader.dst = ethernet_src_address;
      eheader.type = EthernetHeader::TYPE_ARP;
      
      // construct the arp reply message 
      ARPMessage arp_message;
      arp_message.opcode = ARPMessage::OPCODE_REPLY;

      arp_message.sender_ip_address = ip_address_.ipv4_numeric();
      arp_message.sender_ethernet_address = ethernet_address_;

      arp_message.target_ip_address = ip_src_address.value();
      arp_message.target_ethernet_address = ethernet_src_address;
      
      // construct the ethernet frame
      EthernetFrame eframe;
      eframe.header = eheader;
      eframe.payload = serialize(arp_message);

      transmit(eframe);
    } else if (ethernet_header.dst == ethernet_address_ && arpmessage.opcode == ARPMessage::OPCODE_REPLY) {
      // if the arp message is answering for our previous arp request
      // we only need to record it's ethernet_address-ip_address mapping, which is already finished
    }
    
    // update the arp_cache_
    if (ip_src_address.has_value()) {
      arp_cache_[ip_src_address.value()] = arp_entry_(ethernet_src_address);
      // update the queued_datagrams_
      if (queued_datagrams_.find(ip_src_address.value()) != queued_datagrams_.end()) {
        Address next_hop = Address::from_ipv4_numeric(ip_src_address.value());
        for (InternetDatagram dgram : queued_datagrams_[ip_src_address.value()]) {
          send_datagram(dgram, next_hop);
        }
        queued_datagrams_.erase(ip_src_address.value());
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  // Update the arp_cache
  vector<uint32_t> arp_entry_to_delete;
  for(auto& pair : arp_cache_) {
    // be really careful when you are trying to complete the subtraction between two unsigned integars
    if (pair.second.ttl <= ms_since_last_tick) {
      arp_entry_to_delete.push_back(pair.first);
    } else {
      pair.second.ttl -= ms_since_last_tick;
    }
  }
  for(auto ip_address : arp_entry_to_delete) {
    arp_cache_.erase(ip_address);
  }

  // Update the arp_sent
  vector<uint32_t> arp_sent_to_delete;
  for(auto& pair: arp_sent_) {
    if (ms_since_last_tick >= pair.second) {
      arp_sent_to_delete.push_back(pair.first);
    } else {
      pair.second -= ms_since_last_tick;
    }
  }
  for(auto ip_address : arp_sent_to_delete) {
    arp_sent_.erase(ip_address);
  }
}
