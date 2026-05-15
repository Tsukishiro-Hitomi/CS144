#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return next_seqno_absolute_ - ack_seqno_absolute_ ;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutive_retransmission_times_ ;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  uint16_t effective_window_size = window_size_ > 0 ? window_size_ : 1;

  uint64_t cur_seqno_in_flight = sequence_numbers_in_flight();
  uint64_t remaining_window_size = 0;
  if (effective_window_size > cur_seqno_in_flight) {
    remaining_window_size = effective_window_size - cur_seqno_in_flight;
  }

  while (remaining_window_size > 0) {
    TCPSenderMessage msg;
    if (next_seqno_absolute_ == 0) {
      msg.SYN = true;
      remaining_window_size -= 1;
    }

    if (remaining_window_size > 0) {
      string_view view = input_.reader().peek();
      uint64_t payload_length = min({remaining_window_size, static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE), static_cast<uint64_t>(view.size())});
      string payload(view.substr(0, payload_length));
      input_.reader().pop(payload_length);
      msg.payload = payload;
      remaining_window_size -= payload_length;
    }

    if (input_.reader().is_finished()) {
      // we should send valid FIN segment only once, that's why we check fin_has_sent here
      if (!fin_has_sent_ && remaining_window_size > 0) {
        msg.FIN = true;
        remaining_window_size -= 1;
        fin_has_sent_ = true;
      }
    }
    
    // we won't send empty segment.
    if (msg.sequence_length() == 0) {
      return;
    }
    
    msg.seqno = Wrap32::wrap(next_seqno_absolute_, isn_);

    if (input_.has_error()) {
      msg.RST = true;
    }

    transmit(msg);

    // update
    segments_in_flight_.push_back(msg);
    next_seqno_absolute_ += msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.FIN = false;
  msg.SYN = false;
  msg.payload = "";
  msg.RST = input_.has_error();
  msg.seqno = Wrap32::wrap(next_seqno_absolute_, isn_);
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if (msg.RST) {
    input_.reader().set_error();
  }

  // always update the window_size unless cur_ack_seqno > next_seqno_absolute (which means that msg is meaningless)
  if (!msg.ackno.has_value()) {
    window_size_ = msg.window_size;
    return;
  }

  Wrap32 ackno_wrap32 = msg.ackno.value();
  // zero_point: isn_;  checkpoint: next_seqno_absolute_;
  uint64_t cur_ack_seqno_absolute = ackno_wrap32.unwrap(isn_, next_seqno_absolute_ );

  if (cur_ack_seqno_absolute > next_seqno_absolute_ || cur_ack_seqno_absolute <= ack_seqno_absolute_ ) {
    if (cur_ack_seqno_absolute <= ack_seqno_absolute_) {
      window_size_ = msg.window_size;
    }
    return;
  }

  ack_seqno_absolute_ = cur_ack_seqno_absolute;
  window_size_ = msg.window_size;

  // check and update the segments_in_flight, delete those those are acknowledged
  while (!segments_in_flight_.empty()) {
    TCPSenderMessage cur_message = segments_in_flight_.front();
    segments_in_flight_.pop_front();
    Wrap32 cur_seqno_wrap32 = cur_message.seqno;
    uint64_t cur_seqno_absolute = cur_seqno_wrap32.unwrap(isn_, ack_seqno_absolute_);
    uint64_t cur_total_seqno_absolute = cur_seqno_absolute + cur_message.sequence_length();

    if (cur_total_seqno_absolute > ack_seqno_absolute_) {
      segments_in_flight_.push_front(cur_message);
      break;
    }
  }

  // once received a valid ACK: re-initialize the RTO, consecutive_retransmission_times_ and elapsed_time
  cur_RTO_ms_ = initial_RTO_ms_;
  consecutive_retransmission_times_ = 0;
  elapsed_time_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  elapsed_time_ += ms_since_last_tick;

  // everything good, since all segments have been acknowledged
  if (segments_in_flight_.empty()) {
    elapsed_time_ = 0;
    return;
  }

  if (elapsed_time_ >= cur_RTO_ms_) {
    TCPSenderMessage msg = segments_in_flight_.front();
    transmit(msg);

    // update the consecutive_retransmisson_times_, RTO, elapsed_time_
    if (window_size_ > 0) {
      consecutive_retransmission_times_ += 1;
      cur_RTO_ms_ *= 2;
    }

    elapsed_time_ = 0;
  }
}
