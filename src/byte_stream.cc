#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return is_closed_;
}

void Writer::push( string data )
{
  // Your code here.
  uint64_t data_size = data.size();
  uint64_t cur_capacity = available_capacity();
  /* If data_size of cur_capacity is zero, then data should not be pushed since it's an empty string! */
  if ( data_size == 0 || cur_capacity == 0 ) {
    return;
  }
  if ( data_size <= cur_capacity ) {
    container_.push_back( data );
    bytes_pushed_ += data_size;
  } else {
    string sub_data = data.substr( 0, cur_capacity );
    container_.push_back( sub_data );
    bytes_pushed_ += cur_capacity;
  }
  return;
}

void Writer::close()
{
  // Your code here.
  is_closed_ = true;
  return;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  uint64_t bytes_buffered = bytes_pushed_ - bytes_popped_;
  return capacity_ - bytes_buffered;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return is_closed_ && container_.empty();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return bytes_popped_;
}

/* string_view: the start pointer and length of string contained. */
string_view Reader::peek() const
{
  // Your code here.
  if ( !container_.empty() ) {
    return container_.front();
  } else {
    return {};
  }
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t cur_len = len;

  while ( !container_.empty() && cur_len > 0 ) {
    string cur_data = container_.front();
    container_.pop_front();
    if ( cur_data.size() > cur_len ) {
      string cur_data_remained = cur_data.substr( cur_len );
      container_.push_front( cur_data_remained );
      bytes_popped_ += cur_len;
      break;
    } else {
      cur_len -= cur_data.size();
      bytes_popped_ += cur_data.size();
    }
  }
  return;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return bytes_pushed_ - bytes_popped_;
}
