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
  TCPReceiverMessage msg;
  
  // 1. 设置滑动窗口大小
  // 强制转换可用容量，并且由于 uint16_t 最大只有 65535，需要给它设个上限
  uint64_t capacity = writer().available_capacity();
  msg.window_size = capacity > UINT16_MAX ? UINT16_MAX : capacity;
  
  if (writer().has_error()) {
    msg.RST = true;
  }

  // 2. 如果收到过 SYN 包，计算要发给对面的期待序列号 (ackno)
  if ( zero_point_.has_value() ) {
      // 应该是已经成功 pushed 到 stream 中的字节 + 1个 SYN 标志位
      // 别忘了，如果有 FIN 还要再 + 1 
      uint64_t next_absolute_seqno = writer().bytes_pushed() + 1;
      if ( writer().is_closed() ) {
          next_absolute_seqno += 1; // FIN flag 被签收后累加
      }
      msg.ackno = Wrap32::wrap( next_absolute_seqno, zero_point_.value() );
  }

  return msg;
}
