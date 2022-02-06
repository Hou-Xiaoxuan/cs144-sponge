#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _timer(_segments_out, _initial_retransmission_timeout, _isn) {}

uint64_t TCPSender::bytes_in_flight() const { return this->_timer.bytes_in_flight(); }

void TCPSender::fill_window()
{
    /*SYN和FIN都不建议携带数据*/

    // 没有建立连接，发送SYN
    if(this->next_seqno_absolute() == 0)
    {
        TCPSegment seg;
        seg.header().syn = true;
        this->_send_byte(std::move(seg), 0);
        return;
    }
    // 数据发送完毕，但是还没有全部确认 发送FIN
    if(this->_stream.eof()
        and this->next_seqno_absolute() == this->_stream.bytes_written() + 2
        and this->bytes_in_flight() > 0)
    {
        
        TCPSegment seg;
        seg.header().fin = true;
        this->_send_byte(std::move(seg), 0);
        return;
    }

    /*正常发送数据*/
    if(this->_window_size == 0)
    {
        // act asif window-size is 1.
        this->_send_byte(TCPSegment(), 1ul);
    }
    else
    {
        while(this->_window_size > 0u and this->_stream.buffer_size() > 0ul)
        {
            size_t num = min(min(
                static_cast<size_t>(_window_size), 
                this->_stream.buffer_size()), 
                TCPConfig::MAX_PAYLOAD_SIZE);
            if(num > 0ul){
                this->_send_byte(TCPSegment(), num);
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    size_t ack_seqno = unwrap(ackno, this->_isn, this->_timer.last_ack_seqno());
    this->_timer.invoke(ack_seqno);
    this->_window_size = window_size;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick){ 
    this->_timer.update_time(ms_since_last_tick, this->_window_size == 0u);
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return this->_timer.consecutive_retransmissions();
}

void TCPSender::send_empty_segment()
{
    // 发送一个空的segment，不占据sqono，不需要重发
    TCPSegment seg;
    seg.header().seqno = this->next_seqno();
    this->_segments_out.push(seg);
}

void TCPSender::_send_byte(TCPSegment &&seg, const size_t num)
{
    // 填充报文
    if(num > 0ul){
        std::string data = this->_stream.read(num);
        Buffer buf(std::move(data));
        seg.payload() = buf;
    }
    
    // 计算seqno
    seg.header().seqno = this->next_seqno();
    size_t seq_add = seg.length_in_sequence_space();
    this->_timer.push(std::move(seg));
    this->_next_seqno += seq_add;
    this->_window_size -= seq_add;
}







/*
stream_retransmiter
*/
Stream_Retransmiter:: Stream_Retransmiter(queue<TCPSegment> &segments_out, unsigned int &initial_retransmission_timeout, WrappingInt32 &isn):
    _segments_out(segments_out), 
    _initial_retransmission_timeout(initial_retransmission_timeout), 
    _isn(isn),
    rto(_initial_retransmission_timeout)
{ }

// 检测并重发, 返回是否发生了重发
bool Stream_Retransmiter::_check_time()
{
    if(this->_time > this->rto)
    {
        // 超时
        this->_segments_out.push(this->_outgoing_segment.front());
        return true;
    }
    return false;
}

void Stream_Retransmiter:: update_time(size_t time_add, bool window_zero)
{
    this->_time += time_add;
    if(this->_outgoing_segment.empty() == false and this->_check_time())
    {
        this->start();
        if(window_zero == false)
        {
            // 增加连续次数
            this->_consecutive_retransmissions += 1;
            this->rto *= 2ul;
        }
    }
}

void Stream_Retransmiter::invoke(size_t ack_seqno)
{
    // 更新outgoing队列
    bool flag = false;
    while(this->_outgoing_segment.empty() == false)
    {
        auto &seg = this->_outgoing_segment.front();
        size_t seqno = unwrap(seg.header().seqno, this->_isn, this->_last_ack_seqno) + seg.length_in_sequence_space();
        if(ack_seqno >= seqno)
        {
            flag = true;
            size_t sub = this->_outgoing_segment.front().length_in_sequence_space();
            this->_outgoing_segment.pop();
            this->_bytes_in_flight -= sub;
        }
        else
        {
            // 从前往后检查是否已经确认。无法确认时停止
            break;
        }
    }
    if(flag == true)
    {
        this->rto = this->_initial_retransmission_timeout;  // 恢复
        this->_consecutive_retransmissions = 0;             // 归零
        // 重置
        if(this->_outgoing_segment.empty()){
            this->stop();
        }
        else{
            this->start();
        }
    }
}

void Stream_Retransmiter:: start()
{
    // re/start 计时器
    // 已经打开的状态下重置值
    if(this->_is_start == false)
    {
        this->_is_start = true;
        this->_time = 0;
    }
    else
    {
        this->_time = 0;
    }
}

void Stream_Retransmiter:: stop()
{
    this->_is_start = false;
}

void Stream_Retransmiter:: push(const TCPSegment &&seg)
{
    // 发送并放入outgoing队列
    this->_segments_out.push(seg);
    this->_outgoing_segment.push(seg);
    // 增加bytes_in_flight
    this->_bytes_in_flight += seg.length_in_sequence_space();
    if(this->_is_start == false){
        this->start();
    }
}
