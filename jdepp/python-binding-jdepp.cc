#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

// To increase portability, MMAP is off by default.
// #defined JAGGER_USE_MMAP_IO

#include "pdep.h"

#include "io-util.hh"

#ifndef NUM_POS_FIELD
#define NUM_POS_FIELD 4
#endif

namespace py = pybind11;

namespace {

constexpr uint32_t kMaxThreads = 1024;

// ----------------------------------------------------------------------------
// Small vector class useful for multi-threaded environment.
//
// stack_container.h
//
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This allocator can be used with STL containers to provide a stack buffer
// from which to allocate memory and overflows onto the heap. This stack buffer
// would be allocated on the stack and allows us to avoid heap operations in
// some situations.
//
// STL likes to make copies of allocators, so the allocator itself can't hold
// the data. Instead, we make the creator responsible for creating a
// StackAllocator::Source which contains the data. Copying the allocator
// merely copies the pointer to this shared source, so all allocators created
// based on our allocator will share the same stack buffer.
//
// This stack buffer implementation is very simple. The first allocation that
// fits in the stack buffer will use the stack buffer. Any subsequent
// allocations will not use the stack buffer, even if there is unused room.
// This makes it appropriate for array-like containers, but the caller should
// be sure to reserve() in the container up to the stack buffer size. Otherwise
// the container will allocate a small array which will "use up" the stack
// buffer.
template <typename T, size_t stack_capacity>
class StackAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer pointer;
  typedef typename std::allocator<T>::size_type size_type;

  // Backing store for the allocator. The container owner is responsible for
  // maintaining this for as long as any containers using this allocator are
  // live.
  struct Source {
    Source() : used_stack_buffer_(false) {}

    // Casts the buffer in its right type.
    T *stack_buffer() { return reinterpret_cast<T *>(stack_buffer_); }
    const T *stack_buffer() const {
      return reinterpret_cast<const T *>(stack_buffer_);
    }

    //
    // IMPORTANT: Take care to ensure that stack_buffer_ is aligned
    // since it is used to mimic an array of T.
    // Be careful while declaring any unaligned types (like bool)
    // before stack_buffer_.
    //

    // The buffer itself. It is not of type T because we don't want the
    // constructors and destructors to be automatically called. Define a POD
    // buffer of the right size instead.
    char stack_buffer_[sizeof(T[stack_capacity])];

    // Set when the stack buffer is used for an allocation. We do not track
    // how much of the buffer is used, only that somebody is using it.
    bool used_stack_buffer_;
  };

  // Used by containers when they want to refer to an allocator of type U.
  template <typename U>
  struct rebind {
    typedef StackAllocator<U, stack_capacity> other;
  };

  // For the straight up copy c-tor, we can share storage.
  StackAllocator(const StackAllocator<T, stack_capacity> &rhs)
      : source_(rhs.source_) {}

  // ISO C++ requires the following constructor to be defined,
  // and std::vector in VC++2008SP1 Release fails with an error
  // in the class _Container_base_aux_alloc_real (from <xutility>)
  // if the constructor does not exist.
  // For this constructor, we cannot share storage; there's
  // no guarantee that the Source buffer of Ts is large enough
  // for Us.
  // TODO(Google): If we were fancy pants, perhaps we could share storage
  // iff sizeof(T) == sizeof(U).
  template <typename U, size_t other_capacity>
  StackAllocator(const StackAllocator<U, other_capacity> &other)
      : source_(nullptr) {
    (void)other;
  }

  explicit StackAllocator(Source *source) : source_(source) {}

  // Actually do the allocation. Use the stack buffer if nobody has used it yet
  // and the size requested fits. Otherwise, fall through to the standard
  // allocator.
  pointer allocate(size_type n, void *hint = nullptr) {
    if (source_ != nullptr && !source_->used_stack_buffer_ &&
        n <= stack_capacity) {
      source_->used_stack_buffer_ = true;
      return source_->stack_buffer();
    } else {
      return std::allocator<T>::allocate(n, hint);
    }
  }

  // Free: when trying to free the stack buffer, just mark it as free. For
  // non-stack-buffer pointers, just fall though to the standard allocator.
  void deallocate(pointer p, size_type n) {
    if (source_ != nullptr && p == source_->stack_buffer())
      source_->used_stack_buffer_ = false;
    else
      std::allocator<T>::deallocate(p, n);
  }

 private:
  Source *source_;
};

// A wrapper around STL containers that maintains a stack-sized buffer that the
// initial capacity of the vector is based on. Growing the container beyond the
// stack capacity will transparently overflow onto the heap. The container must
// support reserve().
//
// WATCH OUT: the ContainerType MUST use the proper StackAllocator for this
// type. This object is really intended to be used only internally. You'll want
// to use the wrappers below for different types.
template <typename TContainerType, int stack_capacity>
class StackContainer {
 public:
  typedef TContainerType ContainerType;
  typedef typename ContainerType::value_type ContainedType;
  typedef StackAllocator<ContainedType, stack_capacity> Allocator;

  // Allocator must be constructed before the container!
  StackContainer() : allocator_(&stack_data_), container_(allocator_) {
    // Make the container use the stack allocation by reserving our buffer size
    // before doing anything else.
    container_.reserve(stack_capacity);
  }

  // Getters for the actual container.
  //
  // Danger: any copies of this made using the copy constructor must have
  // shorter lifetimes than the source. The copy will share the same allocator
  // and therefore the same stack buffer as the original. Use std::copy to
  // copy into a "real" container for longer-lived objects.
  ContainerType &container() { return container_; }
  const ContainerType &container() const { return container_; }

  // Support operator-> to get to the container. This allows nicer syntax like:
  //   StackContainer<...> foo;
  //   std::sort(foo->begin(), foo->end());
  ContainerType *operator->() { return &container_; }
  const ContainerType *operator->() const { return &container_; }

#ifdef UNIT_TEST
  // Retrieves the stack source so that that unit tests can verify that the
  // buffer is being used properly.
  const typename Allocator::Source &stack_data() const { return stack_data_; }
#endif

 protected:
  typename Allocator::Source stack_data_;
  unsigned char pad_[7];
  Allocator allocator_;
  ContainerType container_;

  // DISALLOW_EVIL_CONSTRUCTORS(StackContainer);
  StackContainer(const StackContainer &);
  void operator=(const StackContainer &);
};

// StackVector
//
// Example:
//   StackVector<int, 16> foo;
//   foo->push_back(22);  // we have overloaded operator->
//   foo[0] = 10;         // as well as operator[]
template <typename T, size_t stack_capacity>
class StackVector
    : public StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                            stack_capacity> {
 public:
  StackVector()
      : StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                       stack_capacity>() {}

  // We need to put this in STL containers sometimes, which requires a copy
  // constructor. We can't call the regular copy constructor because that will
  // take the stack buffer from the original. Here, we create an empty object
  // and make a stack buffer of its own.
  StackVector(const StackVector<T, stack_capacity> &other)
      : StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                       stack_capacity>() {
    this->container().assign(other->begin(), other->end());
  }

  StackVector<T, stack_capacity> &operator=(
      const StackVector<T, stack_capacity> &other) {
    this->container().assign(other->begin(), other->end());
    return *this;
  }

  // Vectors are commonly indexed, which isn't very convenient even with
  // operator-> (using "->at()" does exception stuff we don't want).
  T &operator[](size_t i) { return this->container().operator[](i); }
  const T &operator[](size_t i) const {
    return this->container().operator[](i);
  }
};

// ----------------------------------------------------------------------------

static inline bool is_line_ending(const char *p, size_t i, size_t end_i) {
  if (p[i] == '\0') return true;
  if (p[i] == '\n') return true;  // this includes \r\n
  if (p[i] == '\r') {
    if (((i + 1) < end_i) && (p[i + 1] != '\n')) {  // detect only \r case
      return true;
    }
  }
  return false;
}

struct LineInfo {
  size_t pos{0};
  size_t len{0};
};

using LineInfoVector = StackVector<std::vector<LineInfo>, kMaxThreads>;

//
// Return: List of address of line begin/end in `src`.
//
static LineInfoVector split_lines(const std::string &src,
                                  uint32_t req_nthreads = 0) {
  // From nanocsv. https://github.com/lighttransport/nanocsv

  uint32_t num_threads = (req_nthreads == 0)
                             ? uint32_t(std::thread::hardware_concurrency())
                             : req_nthreads;
  num_threads = (std::max)(
      1u, (std::min)(static_cast<uint32_t>(num_threads), kMaxThreads));

  const size_t buffer_length = src.size();
  const char *buffer = src.c_str();

  LineInfoVector line_infos;
  line_infos->resize(kMaxThreads);

  for (size_t t = 0; t < static_cast<size_t>(num_threads); t++) {
    // Pre allocate enough memory. len / 128 / num_threads is just a heuristic
    // value.
    line_infos[t].reserve(buffer_length / 128 / size_t(num_threads));
  }

  // Find newline('\n', '\r' or '\r\n') and create line data.
  {
    StackVector<std::thread, kMaxThreads> workers;

    auto chunk_size = buffer_length / size_t(num_threads);

    // input is too small. use single-threaded parsing
    if (buffer_length < size_t(num_threads)) {
      num_threads = 1;
      chunk_size = buffer_length;
    }

    for (size_t t = 0; t < static_cast<size_t>(num_threads); t++) {
      workers->push_back(std::thread([&, t]() {
        auto start_idx = (t + 0) * chunk_size;
        auto end_idx = (std::min)((t + 1) * chunk_size, buffer_length - 1);
        if (t == static_cast<size_t>((num_threads - 1))) {
          end_idx = buffer_length - 1;
        }

        // true if the line currently read must be added to the current line
        // info
        bool new_line_found =
            (t == 0) || is_line_ending(buffer, start_idx - 1, end_idx);

        size_t prev_pos = start_idx;
        for (size_t i = start_idx; i < end_idx; i++) {
          if (is_line_ending(buffer, i, end_idx)) {
            if (!new_line_found) {
              // first linebreak found in (chunk > 0), and a line before this
              // linebreak belongs to previous chunk, so skip it.
              prev_pos = i + 1;
              new_line_found = true;
            } else {
              LineInfo info;
              info.pos = prev_pos;
              info.len = i - prev_pos;

              if (info.len > 0) {
                line_infos[t].push_back(info);
              }

              prev_pos = i + 1;
            }
          }
        }

        // If at least one line started in this chunk, find where it ends in the
        // rest of the buffer
        if (new_line_found && (t < size_t(num_threads)) &&
            (buffer[end_idx - 1] != '\n')) {
          for (size_t i = end_idx; i < buffer_length; i++) {
            if (is_line_ending(buffer, i, buffer_length)) {
              LineInfo info;
              info.pos = prev_pos;
              info.len = i - prev_pos;

              if (info.len > 0) {
                line_infos[t].push_back(info);
              }

              break;
            }
          }
        }
      }));
    }

    for (size_t t = 0; t < workers->size(); t++) {
      workers[t].join();
    }
  }

  return line_infos;
}

}  // namespace

namespace pyjdepp {

class PyJdepp {
 public:
  PyJdepp() : _model_loaded{false} {}
  PyJdepp(const std::string &model_path)
      : _model_path(model_path)  {
    load_model(_model_path);
  }

  bool load_model(const std::string &model_path) {
#if 0 // TODO
    if (_model_loaded) {
      // discard previous model&instance.
      delete _tagger;

      _tagger = new jdepp::tagger();
    }

    if (_tagger->read_model(model_path)) {
      _model_loaded = true;
      _model_path = model_path;
      //py::print("Model loaded:", model_path);
    } else {
      _model_loaded = false;
      py::print("Model load failed:", model_path);
    }
#endif

    return _model_loaded;
  }


 private:
  uint32_t _nthreads{0};  // 0 = use all cores
  std::string _model_path;
  //jdepp::tagger *_tagger{nullptr};
  bool _model_loaded{false};
};

}  // namespace pyjdepp

PYBIND11_MODULE(jdepp_ext, m) {
  m.doc() = "Python binding for Jdepp.";

  // Add Ext prefix to avoid name conflict of 'Jdepp' class in Python
  // world(defined in `jdepp/__init__.py`)
  py::class_<pyjdepp::PyJdepp>(m, "JdeppExt")
      .def(py::init<>())
      ;
}
