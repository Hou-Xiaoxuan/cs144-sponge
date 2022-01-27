#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <deque>
/*
* 参考一些中文的翻译和解释，任务大致如下：
- StreamRessembler被用在TCPReviver中，将乱序、会重复的报文段重组，并放入ByteStream中
- 已经排好序的报文需要放入ByteStream中。同时缓存未排好序的报文
- ~~capacity的容量是未排好序的缓存中的加上已经排好序、放入ByteStream中大小~~
- error: capacity理解有误。乱序中，未收到的部分也计入capacity。
  因此，capacity为_buffer和_output的容量和
* 缓存的实现思路：
    吐槽：要是规定滑动窗口的大小就可以不用考虑这个问题了
    1.  用list<char>实现，乱序插入时，list中间填空缺。
        但是，空缺的标记是一个问题。有两种思路：
        - 用list<bool>标记，消耗双倍空间
        - 用heap<pair<int, int>>储存，最坏情况消耗sizeof(int)*2+1倍空间
        另外，插入中间片段的时间是O(n)
    2.  用deque实现。不需要考虑中间存储的问题，但是消耗更多的空间，同时也有标记的问题。
* duplicated.与overlap: 忽略已经确认过的，其余不做检查，直接重复复制、标记。
    * error: 需要检查index是否小于_ack, 此时会造成offset过大，可能越界
* 用_ack标记期待的下一个index。结合deque的索引访问数据
* 注意_output也有容量，可以写入失败
*/
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    std::deque<char> _buffer = std::deque<char>();      // 储存缓存的数据
    std::deque<bool> _vis = std::deque<bool>();         // 标记存储过的数据
    size_t _ack = 0;                                    // 期待的下一个标号
    size_t _unassembled_bytes = 0;                      // 未排好序的数据
    size_t _eof = size_t(-1);                           // 最后一字节的标号

    /*私有接口*/
    // 检查并将有序数据写入_output
    void _make_output();

    // 扩充_buffer以储存新的substring。
    // 检查容量，扩充至容忍的最大
    // target   目标大小
    size_t _extend_buffer(size_t target);

    // 返回已经占据的容量
    size_t _exist_capacity();
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
