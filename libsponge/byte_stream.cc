#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity)
{
    // empty
}

size_t ByteStream::write(const string &data) {
    // 容量不够时，写入到最大
    size_t written = min(this->remaining_capacity(), data.size());
    this->_buffer.append(Buffer(data.substr(0ul, written)));
    this->tot_written += written;
    return written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if(len > this->buffer_size())
    {
        throw std::out_of_range("len biger than size of bytestream");
    }
    auto bufs = this->_buffer.buffers();
    auto ite = bufs.begin();
    string ret = string();

    while(ret.size() < len){
        if(ret.size() + ite->str().size() <= len){
            ret.append(ite->str());
        }
        else {
            ret.append(ite->str().substr(0ul, len - ret.size()));
        }
        ite++;
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if(len > this->buffer_size())
    {
        set_error();
        return;
    }
    this->_buffer.remove_prefix(len);
    this->tot_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if(len > this->buffer_size())
    {
        set_error();
        return "";
    }
    string && ret = this->peek_output(len);
    this->pop_output(len);
    return ret;
}

void ByteStream::end_input() {
    this->is_input_end = true;
}

bool ByteStream::input_ended() const {
    return this->is_input_end;
}

size_t ByteStream::buffer_size() const {
    return this->tot_written - this->tot_read; 
}

bool ByteStream::buffer_empty() const {
    return this->tot_written == this->tot_read;
}

bool ByteStream::eof() const {
    return this->input_ended()?this->buffer_size() == 0ul:false;
}

size_t ByteStream::bytes_written() const { 
    return this->tot_written;
 }

size_t ByteStream::bytes_read() const { 
    return this->tot_read;
}

size_t ByteStream::remaining_capacity() const {
    return this->_capacity - this->buffer_size();
}
