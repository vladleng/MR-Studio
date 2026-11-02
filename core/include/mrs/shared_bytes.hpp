#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include <initializer_list>
#include <utility>

namespace mrs {
// Model/control-thread value. Copies share immutable bytes; mutation replaces
// storage. No writable pointer/iterator escapes into an older Undo snapshot.
// The serialized representation remains the same byte sequence.
class SharedBytes {
    std::shared_ptr<const std::vector<std::byte>> bytes_;
    const std::vector<std::byte>& value() const noexcept {
        static const std::vector<std::byte> empty;
        return bytes_?*bytes_:empty;
    }
public:
    SharedBytes()=default;
    SharedBytes(std::vector<std::byte> bytes):bytes_(std::make_shared<const std::vector<std::byte>>(std::move(bytes))){}
    SharedBytes(std::initializer_list<std::byte> bytes):SharedBytes(std::vector<std::byte>(bytes)){}
    operator const std::vector<std::byte>&() const noexcept {return value();}
    std::size_t size() const noexcept {return value().size();}
    bool empty() const noexcept {return value().empty();}
    const std::byte* data() const noexcept {return value().data();}
    auto begin() const noexcept {return value().begin();}
    auto end() const noexcept {return value().end();}
    const std::byte& operator[](std::size_t at) const {return value()[at];}
    void clear() noexcept {bytes_.reset();}
    void assign(std::size_t n,std::byte byte){*this=SharedBytes(std::vector<std::byte>(n,byte));}
    template<class It>void assign(It first,It last){*this=SharedBytes(std::vector<std::byte>(first,last));}
    void resize(std::size_t n,std::byte byte=std::byte{}){auto copy=value();copy.resize(n,byte);*this=SharedBytes(std::move(copy));}
    bool operator==(const SharedBytes& other) const noexcept {return bytes_==other.bytes_||value()==other.value();}
};
}
