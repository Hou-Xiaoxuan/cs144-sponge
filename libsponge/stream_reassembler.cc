#include "stream_reassembler.hh"
# include<iostream>
# include<vector>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity){}

size_t StreamReassembler::_exist_capacity() const
{
    if(this->_unassembled_buffer.empty()){
        return this->_capacity - this->_output.buffer_size();
    }
    return this->_capacity - (this->_nonexsit_last - this->_ack) - this->_output.buffer_size();
}

void StreamReassembler::_insert(SegmentBlock &&seg)
{
    this->_unassembled_bytes += seg.size();
    if(seg.size()){
        this->_unassembled_buffer.insert(seg);
    }
}

void StreamReassembler::_make_output()
{
    while(this->_unassembled_buffer.empty() == false)
    {
        auto ite = this->_unassembled_buffer.begin();
        if(ite->seqno() == this->_ack){
            this->stream_out().write(string(ite->str()));
            this->_ack = ite->next_seqno();
            this->_unassembled_bytes -= ite->size();
            this->_unassembled_buffer.erase(ite);
        }
        else{
            break;
        }

    }
    // 更新eof
    if(this->_ack == this->_eof){
        this->stream_out().end_input();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    // 已经调试了8小时了，要吐了
    if(eof == true)
    {
        // 到达末尾标记结尾
        // 是否已经写入到_output由_make_output()判断
        this->_eof = index + data.size();
    }
    // TODO: 修剪、插入数据
    SegmentBlock seg(string(data), index); // 唯一一次复制字符串
    size_t max_seqno = this->_ack + this->_capacity - this->_output.buffer_size(); // 最大能容许的序号(不能等于)
    // cout<<"insert ["<<seg.seqno()<<"-"<<seg.next_seqno()<<")"<<endl;
    // 剔除不合法的范围
    if(seg.seqno() >= max_seqno){
        // 全部跳过
        seg.remove_subfix(seg.size());
    }
    else if(seg.next_seqno() > max_seqno){
        seg.remove_subfix(seg.next_seqno() - max_seqno);
    }
    
    if(seg.next_seqno() < this->_ack){
        // 全部跳过
        seg.remove_prefix(seg.size());
    }
    else if(seg.seqno() < this->_ack){
        seg.remove_prefix(this->_ack - seg.seqno());
    }

    // 进行切割并插入
    if(seg.size())
    {
        if(seg.seqno() >= this->_nonexsit_last){
            // 全新片段， 不需要切割
            this->_insert(std::move(seg));
        }
        else
        {
            // 可能需要切割
            auto ite = this->_unassembled_buffer.lower_bound(seg); // index >= seg的第一个

            if(ite == this->_unassembled_buffer.end()) // 没有搜索
            {
                // 特判，新seg的标号是最大的
                if(this->_unassembled_buffer.empty()){
                    this->_insert(std::move(seg));
                }
                else if(seg.next_seqno() > this->_nonexsit_last){
                    seg.remove_prefix(this->_nonexsit_last - seg.seqno());
                    this->_insert(std::move(seg));
                }
            }
            else
            {
                // !不要在使用迭代器过程中改变set, 先缓存下来
                std::vector<SegmentBlock> tmp_cache{};
                if(ite != this->_unassembled_buffer.begin()) ite--;    // index < seg的第一个
                while(seg.size() and ite != this->_unassembled_buffer.end())
                {
                    // seg_l >= ite_r 无重叠
                    if(seg.seqno() >= ite->next_seqno()){
                        // empty
                    }
                    // seg_l < ite_r and seg_l > ite_l 左端有一部分重叠的
                    else if(seg.seqno() >= ite->seqno()){
                        seg.remove_prefix(std::min(seg.size(), ite->next_seqno() - seg.seqno()));
                    }
                    // seg_l < ite_r and seg_l < ite_l and seg_l < ite_r and seg_r > ite_l 左侧有需要insert的部分，右侧有重叠
                    else if(seg.next_seqno() > ite->seqno()){
                        SegmentBlock tmp(seg);
                        tmp.remove_subfix(tmp.next_seqno() - ite->seqno());
                        // this->_insert(std::move(tmp));
                        tmp_cache.push_back(std::move(tmp));
                        seg.remove_prefix(std::min(seg.size(), ite->next_seqno() - seg.seqno()));
                    }
                    // seg_r < ite_l 两者完全无重叠
                    else {
                        SegmentBlock tmp(seg);
                        // this->_insert(std::move(tmp));
                        tmp_cache.push_back(std::move(tmp));

                        seg.remove_prefix(seg.size());
                    }
                    ite++;
                }
                if(seg.size() > 0){
                    tmp_cache.push_back(seg);
                }
                while(tmp_cache.empty() == false){
                    this->_insert(std::move(tmp_cache.back()));
                    tmp_cache.pop_back();
                }
            }
        }
    }
    // 更新数据
    if(this->_unassembled_buffer.empty() == false){
        this->_nonexsit_last = std::max(this->_nonexsit_last, this->_unassembled_buffer.rbegin()->next_seqno());
    }
    // // debug
    // cout<<"debug "<<this->stream_out().bytes_written()<<"-"<<this->_unassembled_buffer.size()<<endl;
    // for(auto ite: this->_unassembled_buffer){
    //     cout<<": ["<<ite.seqno()<<"-"<<ite.next_seqno()<<") ";
    // }
    // cout<<endl<<endl;
    this->_make_output();
}

size_t StreamReassembler::unassembled_bytes() const { return this->_unassembled_bytes; }

bool StreamReassembler::empty() const { return this->unassembled_bytes() == 0ul; }
