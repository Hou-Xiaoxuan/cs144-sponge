#include "stream_reassembler.hh"
# include<iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity){}

void StreamReassembler::_make_output()
{
    // ！考虑异常安全性
    string to_write("");
    for(size_t i = 0; i<this->_buffer.size(); i++)
    {
        if(this->_vis[i])
        {
            to_write.push_back(this->_buffer[i]);
        }
        else
        {
            break;
        }
    }
    size_t written = this->_output.write(to_write);

    // 仅删除确保写入的部分
    while(written--)
    {
        this->_buffer.pop_front();
        this->_vis.pop_front();
        this->_ack++;
        this->_unassembled_bytes--;
    }

    // 检查是否eof
    if(this->_ack >= this->_eof)
    {
        // 停止写入
        this->_output.end_input();
    }
}

size_t StreamReassembler::_extend_buffer(size_t target)
{

    while(target > this->_buffer.size() and this->_exist_capacity()<this->_capacity)
    {
        this->_buffer.push_back(' ');
        this->_vis.push_back(false);
    }
    return this->_buffer.size();
}

size_t StreamReassembler::_exist_capacity()
{
    return this->_output.buffer_size() + this->_buffer.size();
}
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    if(eof == true)
    {
        // 到达末尾标记结尾
        // 是否已经写入到_output由_make_output()判断
        this->_eof = index + data.size();
    }

    // 检查数据是否已经排序，从data的start位置开始插入。
    size_t start = 0ul;
    if(index < this->_ack)
    {
        if(index + data.size() < this->_ack){
            start = data.size();
        }
        else{
            start = this->_ack - index;
        }
    }
    size_t offset = index - this->_ack;

    size_t cap = this->_extend_buffer(offset + data.size());

    for(size_t i = start; i<data.size() and i+offset < cap; i++)
    {
        
        // 放入数据
        this->_buffer[offset + i] = data[i];
        if(this->_vis[offset + i] == false)
        {
            // 标记染色
            this->_vis[offset + i] = true;
            // 增加记录
            this->_unassembled_bytes++;
        }
    }
    this->_make_output();
}

size_t StreamReassembler::unassembled_bytes() const { return this->_unassembled_bytes; }

bool StreamReassembler::empty() const { return this->_unassembled_bytes == 0ul; }
