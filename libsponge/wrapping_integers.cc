#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn)
{
    // 32位加法自动取模
    return WrappingInt32(uint32_t(n) + isn.raw_value());
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint)
{
    // error
    // uint64_t ret = uint64_t(n.raw_value() - isn.raw_value()) + ((checkpoint>>32ul)<<32ul);
    // // 执行判断大小容易出错 1 < (1<<64)，但是实际上1是溢出的结果
    // // 执行到这里的初步的ret一定是接近checkpoint的。
    // // ret<=(1<<32 - 1)，要么checkpoint很小，要么很大，溢出了64位
    // // ret>(1<<32 - 1)，则一定是增加了一个小于checkpoint的数，才达到的值
    // // TODO::临界的判断是错的
    // if(ret < (1ul<<32ul) and ret > checkpoint + (1ul<<31ul)){
    //     ret -= (1ul<<32ul);
    // }
    // else if(ret >= (1ul<<32ul) and ret < checkpoint - (1ul<<31ul)){
    //     ret += (1ul<<32ul);
    // }
    uint64_t compar = checkpoint - ((checkpoint>>32ul)<<32ul);
    uint64_t ret = uint64_t(n.raw_value() - isn.raw_value());
    if(ret > compar and (ret - compar) > (1ul<<31ul) and checkpoint >= (1ul<<32ul)){
        ret -= (1ul<<32ul);

    }
    else if(ret < compar and (compar - ret) > (1ul<<31ul)){
        ret += (1ul<<32ul);
    }
    ret += ((checkpoint>>32ul)<<32ul);
    return ret;
}
