#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include<vector>
// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address){
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop)
{
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 生成报文
    auto ip_to_ethernet = this->_cache.find(next_hop_ip);
    if(ip_to_ethernet != this->_cache.end())
    {
        // 发送报文
        this->_send_internet_message(ip_to_ethernet->second.first, dgram);
    }
    else
    {
        // 放入缓存
        this->_frames_que[next_hop_ip].push(dgram);

        // 发送请求
        auto request_item = this->_requested_que.find(next_hop_ip);
        if(request_item == this->_requested_que.end())
        {
            if(this->_send_arp_message(next_hop_ip, true) == true){
                this->_requested_que.insert(make_pair(next_hop_ip, 0ul));
            }
            // ARPMessage arp_request;
            // arp_request.sender_ethernet_address = this->_ethernet_address;
            // arp_request.sender_ip_address = this->_ip_address.ipv4_numeric();
            
            // EthernetFrame frame;
            // frame.header().src = this->_ethernet_address;
            // frame.header().dst = ETHERNET_BROADCAST;
            // if(frame.parse(arp_request.serialize()) != ParseResult::NoError){
            //     this->_frames_out.push(std::move(frame));
            //     this->_requested_que.insert(make_pair(next_hop_ip, 0ul));
            // }
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    if(frame.header().dst != ETHERNET_BROADCAST and frame.header().dst != this->_ethernet_address){
        return {};
    }

    if(frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        // IP数据报
        InternetDatagram message;
        auto result = message.parse(frame.payload());
        if(result == ParseResult::NoError){
            return message;
        }

    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP)
    {
        // ARP数据报
        ARPMessage message;
        auto result = message.parse(frame.payload());
        if(result == ParseResult::NoError)
        {
            // 更新缓存
            this->_update_cache(message.sender_ip_address, message.sender_ethernet_address);

            // 请求自己的地址，构造回复
            if(message.opcode == ARPMessage::OPCODE_REQUEST and message.target_ip_address == this->_ip_address.ipv4_numeric())
            {
                this->_send_arp_message(message.sender_ip_address, false);
            }         
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) 
{
    // 清理超时的cache
    vector<decltype(this->_cache.begin())> clear_cache;
    for(auto ite = this->_cache.begin(); ite != this->_cache.end(); ite++)
    {
        if(ite->second.second + ms_since_last_tick > 30000ul){
            clear_cache.push_back(ite);
        }
        else{
            ite->second.second += ms_since_last_tick;
        }
    }
    for(auto clear_item: clear_cache){
        this->_cache.erase(clear_item);
    }

    // 重发超时的request
    for(auto ite = this->_requested_que.begin(); ite != this->_requested_que.end(); ite++)
    {
        if(ite->second + ms_since_last_tick > 5000ul){
            // 重发， 并重新计时
            ite->second = 0;
            this->_send_arp_message(ite->first, true);
        }else{
            ite->second += ms_since_last_tick;
        }
    }
}


void NetworkInterface::_update_cache(const uint32_t ip, const EthernetAddress ethernet)
{
    this->_cache[ip] = make_pair(ethernet, 0ul);

    auto item = this->_frames_que.find(ip);
    if(item != this->_frames_que.end())
    {
        while(item->second.empty() == false)
        {
            this->_send_internet_message(ethernet, item->second.front());
            item->second.pop();
        }
        // 删除无用项
        this->_frames_que.erase(item);
    }

    // 清理request_que
    auto clear_item = this->_requested_que.find(ip);
    if(clear_item != this->_requested_que.end()){
        this->_requested_que.erase(clear_item);
    }
}

// 向目的ip发送type类型的ARP报文
bool NetworkInterface:: _send_arp_message(const uint32_t ip, const bool is_request)
{
    ARPMessage message;
    message.sender_ip_address = this->_ip_address.ipv4_numeric();
    message.sender_ethernet_address = this->_ethernet_address;
    message.target_ip_address = ip;
    // Note: 按理来说这里应该是广播地址,但是貌似测试期望为0000
    // message.target_ethernet_address = ethernet;
    message.target_ethernet_address = is_request ? EthernetAddress{0, 0, 0, 0, 0, 0} : this->_cache[ip].first;
    message.opcode = is_request ? ARPMessage::OPCODE_REQUEST : ARPMessage::OPCODE_REPLY;

    EthernetFrame frame;
    frame.header().src = this->_ethernet_address;
    frame.header().dst = is_request ? ETHERNET_BROADCAST : this->_cache[ip].first;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.payload() = std::move(message.serialize());
    this->_frames_out.push(std::move(frame));
    return true;
}

bool NetworkInterface::_send_internet_message(const EthernetAddress &ethernet,const InternetDatagram &dgram)
{
    EthernetFrame frame;
    frame.header().src = this->_ethernet_address;
    frame.header().dst = ethernet;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = std::move(dgram.serialize());
    this->_frames_out.push(std::move(frame));
    return true;
}
