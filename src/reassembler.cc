#include "reassembler.hh"
#include <vector>
using namespace std;
bool is_overlapped( uint64_t first_index1, uint64_t end_index1, uint64_t first_index2, uint64_t end_index2 )
{
  if ( end_index1 + 1 < first_index2 || first_index1 > end_index2 + 1 ) { // if the boundaries of two strings are touched, then it's still overlapped because we can merge them
    return false;
  }
  return true;
}

uint64_t Reassembler::get_bytes_unaccessible() const
{
  return reader().bytes_popped() + capacity_;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  uint64_t bytes_unaccessible_ = get_bytes_unaccessible();  

  if ( is_last_substring ) {
    has_last_ = true;
    final_index_ = first_index + data.size();
  }

  if ( first_index >= bytes_unaccessible_ ) {
    return;
  }

  // empty data
  if ( data.size() == 0 ) {
    if ( has_last_ && bytes_read_ == final_index_ ) {
      output_.writer().close();
    }
    return;
  }

  // if we meet the data that has been read, check and truncate it
  if ( first_index < bytes_read_ ) {
    uint64_t diff = bytes_read_ - first_index;

    if ( has_last_ && bytes_read_ == final_index_ ) {
      output_.writer().close();
    }

    if (diff > 0) {
      data = data.substr( diff );
      first_index = bytes_read_;
    }
  }

  uint64_t end_index = first_index + data.size() - 1;

  // if data is too long, then truncate it
  if ( end_index >= bytes_unaccessible_ ) {
    data = data.substr( 0, bytes_unaccessible_ - first_index );
    end_index = bytes_unaccessible_ - 1;
  }

  vector<pair<uint64_t, string>> overlapped;
  uint64_t result_first_index = first_index;
  uint64_t result_end_index = end_index;

  // capture all overlapped data except the cur one
  for ( auto& pair : buffer ) {
    string cur_data = pair.second
    uint64_t cur_first_index = pair.first;
    uint64_t cur_end_index = cur_first_index + cur_data.size() - 1;
    if ( is_overlapped( cur_first_index, cur_end_index, first_index, end_index ) ) {
      overlapped.push_back( pair );
      result_end_index = max( result_end_index, cur_end_index );
      result_first_index = min( result_first_index, cur_first_index );
    }
  }

  // merge overlapped data
  uint64_t merged_len = result_end_index - result_first_index + 1;
  string merged_data( merged_len, '\0' );
  for ( auto& pair : overlapped ) {
    uint64_t cur_first_index = pair.first;
    string cur_data = pair.second;
    buffer.erase( cur_first_index );  // remember to delete the origin one from the buffer
    bytes_pend_ -= cur_data.size();
    for ( size_t i = 0; i < cur_data.size(); i++ ) {
      size_t offset = cur_first_index - result_first_index + i;
      merged_data[offset] = cur_data[i];
    }
  }

  // process the cur data
  for ( size_t i = 0; i < data.size(); i++ ) {
    size_t offset = first_index - result_first_index + i;
    merged_data[offset] = data[i];
  }

  // update the bytes-pend and buffer
  buffer[result_first_index] = merged_data;
  bytes_pend_ += merged_data.size();

  auto it = buffer.find( bytes_read_ );
  if ( it != buffer.end() ) {
    string send_data = it->second;
    writer().push( send_data );
    buffer.erase( it );
    bytes_read_ += send_data.size();
    bytes_pend_ -= send_data.size();

    if ( has_last_ && bytes_read_ == final_index_ ) {
      writer().close();
    }
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return bytes_pend_;
}
