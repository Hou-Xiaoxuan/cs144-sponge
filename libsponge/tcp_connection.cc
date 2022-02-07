#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return this->_sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return this->_sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return this->_receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return this->_time_now - this->_last_receive_time; }

void TCPConnection::_clear_segs()
{
    auto ackno = this->_receiver.ackno();
    auto win = this->_receiver.window_size();
    while(this->_sender.segments_out().empty() == false)
    {
        auto seg = this->_sender.segments_out().front();
        seg.header().ackno = ackno.has_value()?ackno.value(): WrappingInt32(0ul);
        seg.header().win = win;
        this->_segments_out.push(seg);
        this->_sender.segments_out().pop();
    }
}

void TCPConnection::_send_rst()
{
    // 确保有seg
    this->_sender.send_empty_segment();
    auto seg = this->_sender.segments_out().front();
    seg.header().rst = true;
    this->_segments_out.push(seg);
}

void TCPConnection::_check_active()
{
    if(this->_sender.stream_in().input_ended() and this->_receiver.stream_out().input_ended())
    {
        // 已经全部发送/结束完数据并确认
        if(this->_linger_after_streams_finish == true)
        {
            if(this->time_since_last_segment_received() >= size_t(this->_cfg.rt_timeout)*10){
                // 结束
                this->_active = false;
            }
        }
        else
        {
            // 直接结束
            this->_active = false;
        }
    }
}

void TCPConnection::segment_received(const TCPSegment &seg)
{ 
    if(this->active() == false){
        // 已经终止了
        return;
    }

    if(seg.header().rst == true){
        // 立刻终止
        this->_active = false;
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        return;
    }

    if (this->_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == this->_receiver.ackno().value() - 1) {
        // keep-alive
        this->_sender.send_empty_segment();
    }

    this->_last_receive_time = this->_time_now; // 更新时间
    
    this->_sender.ack_received(seg.header().ack? seg.header().ackno: WrappingInt32(0ul), seg.header().win); // 获取信息
    this->_receiver.segment_received(seg); // 接受报文

    // 发送可能的报文
    this->_clear_segs();

    /*判断是否达成#1～#3条件*/
    if(this->_receiver.stream_out().input_ended() == true and this->_sender.stream_in().input_ended() == false){
        // 数据已经全部被确认，结束后无需等待
        this->_linger_after_streams_finish = false;
    }

    // 检测是否结束
    this->_check_active();
}

bool TCPConnection::active() const { return this->_active; }

size_t TCPConnection::write(const string &data)
{
    size_t tmp = this->_sender.stream_in().write(data);
    // 写入立刻发送
    this->_sender.fill_window();
    this->_clear_segs();

    return tmp;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{ 
    /* 时间改变：最复杂的逻辑了，要控制大部分状态
    * error: 连续重传超过次数限制
    * end: 满足#1～#3后两种情况：
    *   其中，_sender的close一定发生在segment_receive()后才能确认对方受到
    *        _receiver的确认也一定发生在segment_receive()后，才能确认对方没有受到
    *       1. _linger_after_streams_finish == true, 则需要累计等待时间，并判断是否达到
    *           （该标志在receive结束，send未结束时置为false, 需要在receive_seg()后进行判断）
    *           一定发生在tick()这里。时间增加才能确认
    *       2. 无需等待，立即断开
    *           一定发生在segment_receive()中，结束报文后就可以立刻判断
    *       为了避免分散，写到一个函数里
    */
   // 累加时间
   this->_time_now += ms_since_last_tick;

    // 激活_sender
    this->_sender.tick(ms_since_last_tick);

    // 判断是否断开连接
    if(this->_sender.consecutive_retransmissions() >= 10)
    {
        // 发送rto断开连接
        this->_send_rst();
    }
    // 发送可能的数据报
    this->_clear_segs();

    // 检测是否结束
    this->_check_active();
}

void TCPConnection::end_input_stream(){
    this->_sender.stream_in().end_input();
}

void TCPConnection::connect()
{
    this->_active = true;
    // 发送一个SYN
    this->_sender.fill_window();
    // 立即发送
    this->_clear_segs();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            this->_send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
