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
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { DUMMY_CODE(ackno, window_size); }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}


/*
stream_retransmiter*/
Stream_Retransmiter:: Stream_Retransmiter(queue<TCPSegment> &segments_out, unsigned int &initial_retransmission_timeout, WrappingInt32 &isn):
    _segments_out(segments_out), 
    _initial_retransmission_timeout(initial_retransmission_timeout), 
    _isn(isn),
    rto(_initial_retransmission_timeout)
{ }

// 检测并重发, 返回是否发生了重发
bool Stream_Retransmiter::_check_time()
{
    if(this->_timer > this->rto)
    {
        // 超时
        this->_segments_out.push(this->_outgoing_segment.front());
        return true;
    }
    return false;
}

void Stream_Retransmiter:: update_time(size_t time_add, bool window_zero)
{
    this->_timer += time_add;
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
        if(ack_seqno > seqno)
        {
            flag = true;
            this->_outgoing_segment.pop();
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
        this->_timer = 0;
    }
    else
    {
        this->_timer = 0;
    }
}

void Stream_Retransmiter:: stop()
{
    this->_is_start = false;
}

void Stream_Retransmiter:: push(const TCPSegment &seg)
{
    this->_outgoing_segment.push(seg);
    if(this->_is_start == false){
        this->start();
    }
}
