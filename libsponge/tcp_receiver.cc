#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg)
{
    if(this->_sin == false){
        this->_set_sin(seg);
    }
    if(seg.header().fin == true){
        this->_set_fin(seg);
    }
    if(seg.length_in_sequence_space() > 0ul){
        this->_recieve_bytes(seg);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(this->_fin == false){
        return optional<WrappingInt32>();
    }
    return optional<WrappingInt32>(this->_ackno);
}

size_t TCPReceiver::window_size() const { 
    return this->_capacity - this->_reassembler.unassembled_bytes();
}
