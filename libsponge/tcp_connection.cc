#include "tcp_connection.hh"

#include <iostream>
#include <limits>
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
        // Note: seg的ack也是需要手动设置的！
        if(ackno.has_value()){
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
        }
        seg.header().win = min<size_t>(win, std::numeric_limits<uint16_t>().max());
        cout<<"发送一个数据包"<<": "<<seg.header().to_string()<<endl;
        this->_segments_out.push(seg);
        this->_sender.segments_out().pop();
    }
}

void TCPConnection::_send_rst()
{
    // 确保有seg
    cout<<"发送reset"<<endl;
    this->_sender.send_empty_segment();
    auto seg = this->_sender.segments_out().front();
    seg.header().rst = true;
    this->_segments_out.push(seg);
}

void TCPConnection::_check_active()
{   
    // 判断条件过于宽泛
    if(this->_sender.stream_in().eof() and this->_sender.bytes_in_flight() == 0ul and this->_receiver.stream_out().input_ended())
    {
        // 已经全部发送/结束完数据并确认
        if(this->_linger_after_streams_finish == true)
        {
            if(this->time_since_last_segment_received() >= size_t(this->_cfg.rt_timeout)*10ul){
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
    if(_active == false){
        cout<<"结束了"<<endl;
    }
}

void TCPConnection::segment_received(const TCPSegment &seg)
{ 
    /* 补充并明晰结束报文的工作
    * 1. 需要区分close状态和listen状态，初始状态应该接受报文，active为true
    * 2. 需要接受到一个SYN后才能接受后续报文，_receiver做了这样的保证
    * 3. 在收到SYN后应该回复报文以建立连接
    */
   cout<<"接收到报文"<<endl;
   // CLOSE
    if(this->active() == false){
        // 已经终止了
        return;
    }

    // EEEOE
    if(seg.header().rst == true){
        // 立刻终止
        this->_active = false;
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        return;
    }

    // KEEP_ALIVE
    if (this->_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == this->_receiver.ackno().value() - 1) {
        // keep-alive
        this->_sender.send_empty_segment();
    }

    // ALIVE: 正常收发
    this->_last_receive_time = this->_time_now; // 更新时间

    this->_receiver.segment_received(seg); // 接受报文
    
    if(seg.header().ack == true){ // 这里应该总是为真
        // 没有ack则不调用ack_received
        this->_sender.ack_received(seg.header().ackno, seg.header().win); // 获取信息
    }

    // !! 回复ACK报文 // 吐槽：一直没回复，连接能建立起来就见鬼了
    if(seg.length_in_sequence_space() > 0ul){
        this->_sender.send_empty_segment();
    }

    // 发送可能的报文
    this->_clear_segs();

    // LISTEN: 还没有发送过SYN, 但是已经受到了SYN,需要主动向对方建立连接
    if(this->_sender.next_seqno_absolute() == 0ul and this->_receiver.ackno().has_value())
    {
        this->connect();
    }


    /*判断是否达成#1～#3条件*/
    // Note: 判断条件错了。 eof <=> input_ended()?buffer_size() == 0ul:false
    if(this->_receiver.stream_out().input_ended() == true and this->_sender.stream_in().eof() == false){
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
    if(this->_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS)
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
    // 清空数据，并且可以发送fin
    this->_sender.fill_window();
    this->_clear_segs();
}

void TCPConnection::connect()
{   
    cout<<"发起建立连接"<<endl;
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
