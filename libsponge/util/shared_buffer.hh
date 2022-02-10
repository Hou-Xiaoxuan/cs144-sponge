#ifndef SPONGE_LIBSPONGE_SHARED_BUFFER_HH
#define SPONGE_LIBSPONGE_SHARED_HH
/*
 * @Author: LinXuan
 * @Date: 2022-02-10 17:11:24
 * @Description: 
 * @LastEditors: LinXuan
 * @LastEditTime: 2022-02-10 17:58:07
 * @FilePath: /sponge/libsponge/util/shared_buffer.hh
 */
// 全部函数在头文件实现
/* 一个维护两个index，和一个share_ptr，实现高性能字符串只读字符串的buffer
* starting 和 ending 左闭右开
* 复制buffer只复制指针，不复制原生字符串。所有buffer析构后，由share_ptr实现数据析构
* 可以方便的删除前后的数据
* 实现参考 @尚戈继 的文章 https://zhuanlan.zhihu.com/p/414279516 和cs144提供的buffer
*/
# include<iostream>
# include<memory>

class SharedBuffer
{
private:
    std::shared_ptr<std::string> _storage{};
    size_t _starting{};
    size_t _ending;
public: 
    SharedBuffer() = default;
    SharedBuffer(std::string &&s) noexcept:_storage(std::make_shared<std::string>(std::move(s))), _ending(_storage->size()){}
    SharedBuffer(const SharedBuffer & another_buffer):
        _storage(another_buffer._storage),
        _starting(another_buffer._starting),
        _ending(another_buffer._ending){}
    
    // 只读访问
    std::string_view str() const{
        if(not this->_storage){
            return {};
        }
        return std::string_view(this->_storage->data() + this->_starting, this->_ending - this->_starting);
    }

    // 访问字符
    uint8_t at(const size_t n) const {return this->str().at(n);}

    // 应该指向的大小，而不是原生字符串的大小
    size_t size() const {return this->_ending - this->_starting; }

    // 从前/后移除n个字符
    void remove_prefix(const size_t n){
        if(n > this->size()){
            throw std::out_of_range("sharedBuffer:: remove_prefix");
        }
        this->_starting += n;
        if(_storage and  this->size() == 0ul){
            // 删完了
            this->_storage.reset();
        }
    }
    void remove_subfix(const size_t n){
        if(n > this->size()){
            throw std::out_of_range("sharedBuffer:: remove_prefix");
        }
        this->_ending -= n;
        if(_storage and  this->size() == 0ul){
            // 删完了
            this->_storage.reset();
        }
    }
    
    // 生成一份字符串拷贝
    std::string copy(){
        return std::string(this->str());
    }
};

# endif
