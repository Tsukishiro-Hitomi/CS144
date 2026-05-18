#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if (message.RST) {
    reader().set_error();
    return;
  }

  Wrap32 seqno = message.seqno;
  if (message.SYN) {
    zero_point_ = seqno;
  }
  if (!zero_point_.has_value()) {
    return;
  }
  string data = message.payload;
  uint64_t pushed_bytes = reassembler_.writer().bytes_pushed();

  uint64_t checkpoint = pushed_bytes + 1;
  uint64_t abs_seqno = seqno.unwrap(zero_point_.value(), checkpoint);
  /* According to the formula: sequence_length = SYN + payload.size() + FIN;
      the valid SYN will take 1 sequence number, thus, when computing the stream_index, we need to substract 1 because
      SYN is not actually pushed to the assembler. 
  */
  uint64_t stream_index = abs_seqno - 1 + message.SYN;

  reassembler_.insert(stream_index, data, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // Send the ACK segment
  TCPReceiverMessage msg;

  uint64_t capacity = writer().available_capacity();
  msg.window_size = capacity > UINT16_MAX ? UINT16_MAX : capacity;
  
  if (writer().has_error()) {
    msg.RST = true;
  }

  if ( zero_point_.has_value() ) {
      uint64_t next_absolute_seqno = writer().bytes_pushed() + 1;

      // FIN flag is included in the sequence number but not pushed into byte stream.
      if ( writer().is_closed() ) {
          next_absolute_seqno += 1;
      }
      msg.ackno = Wrap32::wrap( next_absolute_seqno, zero_point_.value() );
  }

  return msg;
}
