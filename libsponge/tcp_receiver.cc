#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::_set_syn(const TCPSegment &seg)
{
    // 设置initial segment number
    // sequence number start from the isn
    // isn is the first seq-no, set now
    // ack is the next byte wanted--now, is the first seq-no
    if(seg.header().syn){
        this->_isn = seg.header().seqno;
        this->_syn = true;
    }
}

void TCPReceiver::segment_received(const TCPSegment &seg)
{
    if(this->_syn == false){
        this->_set_syn(seg);
    }

    if(this->ackno().has_value())
    {
        // start receive bytes, calculate and change the ackno
        // note: length_in_sequence_space()的接口已经累加了SIN和FIN的量
        // note: 通过payload()返回read-only的buffer来获取数据
        // note: 通过循环寻找ByteStream的byte_writen()来得到已经写入的byte数量——排好序的数量
        auto buf = seg.payload();
        bool flag = seg.header().fin;
        // 从segno->absolutly-segno->stream_index
        // 计算index时需要-1, 是SYN占据的量(FIN后不需要计算index了)
        size_t checkpoint = this->stream_out().bytes_written();
        size_t index = unwrap(seg.header().seqno, this->_isn, checkpoint);
        if(seg.header().syn == false){
            index -= 1ul; // 减去syn的量。第一个segment不考虑
        }
        this->_reassembler.push_substring(buf.copy(), index, flag);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(this->_syn == false){
        return optional<WrappingInt32>();
    }
    // 根据reseambled计算
    size_t acki = this->_reassembler.ack();
    if(true == this->stream_out().input_ended()){
        return wrap(acki + 2, this->_isn);
    } else{
        return wrap(acki + 1, this->_isn);
    }
}

size_t TCPReceiver::window_size() const { 
    return this->_capacity - this->stream_out().buffer_size();
}
