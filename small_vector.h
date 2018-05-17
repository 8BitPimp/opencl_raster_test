#pragma once
#include <cassert>


template <typename type_t, size_t elements>
struct small_vector {

  small_vector() : _head(0) {}

  struct itter_t {

    type_t &operator*() { return *_ptr; }

    type_t &operator->() { return *_ptr; }

    const type_t &operator*() const { return *_ptr; }

    const type_t &operator->() const { return *_ptr; }

    itter_t(type_t *t) : _ptr(t) {}

    bool operator==(const itter_t &other) const { return _ptr == other._ptr; }

    bool operator!=(const itter_t &other) const { return _ptr != other._ptr; }

    void operator++() { ++_ptr; }

  protected:
    type_t *_ptr;
  };

  itter_t end() { return itter_t{_data.data() + _head}; }

  itter_t begin() { return itter_t{_data.data()}; }

  void push_back(type_t &v) { _data[_head++] = v; }

  type_t &emplace_back() { return _data[_head++]; }

  void resize(uint32_t entries) {
    assert(entries < size);
    _head = entries;
  }

  type_t &front() {
    assert(_head > 0);
    return _data[0];
  }

  type_t &back() {
    assert(_head > 0);
    return _data[_head - 1];
  }

  type_t &operator[](int32_t index) {
    assert(index >= 0 && index < size);
    return _data[index];
  }

  const type_t &operator[](int32_t index) const {
    assert(index >= 0 && index < size);
    return _data[index];
  }

  size_t size() const { return _head; }

  size_t capacity() const { return element; }

  void clear() { _head = 0; }

protected:
  size_t _head;
  std::array<type_t, elements> _data;
};
