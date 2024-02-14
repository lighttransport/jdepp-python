#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <iostream>
#include <sstream>

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

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


#if 0 // not used atm.
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
#endif

}  // namespace

namespace pyjdepp {

namespace {

// compute length of UTF8 character *p
static inline int u8_len (const char *p) {
  static const uint8_t u8bytes[256] = { // must be static to tame compilers
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6
  };
  return u8bytes[static_cast <uint8_t> (*p)];
}

// Support quoted string'\"' (do not consider `delimiter` character in quoted string)
// delimiter must be a ASCII char.
// quote_char must be a single UTF-8 char.
inline std::vector<std::string> parse_feature(const char *p, const size_t len, const char delimiter = ',', const char *quote_char = "\"")
{
  std::vector<std::string> tokens;

  if (len == 0) {
    return tokens;
  }

  size_t quote_size = u8_len(quote_char);

  bool in_quoted_string = false;
  size_t s_start = 0;

  const char *curr_p = p;

  for (size_t i = 0; i < len; i += u8_len(curr_p)) {

    curr_p = &p[i];

    if (is_line_ending(p, i, len - 1)) {
      break;
    }

    if ((i + quote_size) < len) {
      if (memcmp(curr_p, quote_char, quote_size) == 0) {
        in_quoted_string = !in_quoted_string;
        continue;
      }
    }

    if (!in_quoted_string && (p[i] == delimiter)) {
      //std::cout << "s_start = " << s_start << ", (i-1) = " << i-1 << "\n";
      //std::cout << "p[i] = " << p[i] << "\n";
      if (s_start < i) {
        std::string tok(p + s_start, i - s_start);

        tokens.push_back(tok);
      } else {
        // Add empty string
        tokens.push_back(std::string());
      }

      s_start = i + 1; // next to delimiter char
    }
  }

  // the remainder
  //std::cout << "remain: s_start = " << s_start << ", len - 1 = " << len-1 << "\n";

  if (s_start <= (len - 1)) {
    std::string tok(p + s_start, len - s_start);
    tokens.push_back(tok);
  }

  return tokens;
}

} // namespace

class PyToken
{
 public:
  PyToken() = default;
  PyToken(const std::string &surf, const std::string &feature, const double prob = 0.0) : _surface(surf), _feature(feature), _chunk_start_probability(prob) {}

  const std::string &surface() const { return _surface; };
  const std::string &feature() const { return _feature; }
  const double chunk_start_prob() const { return _chunk_start_probability; }

  void set_feature_separator(const char delim) {
    _delim = delim;
  }

  void set_surface_end(const std::string &s) {
    _separator = s;
  }

  void set_quote_char(const std::string &qc) {
    _quote_char = qc;
  }

  const int n_tags() const {

    if (_tags.empty()) {
      // TODO: Specify separator at runtime.
      _tags = parse_feature(_feature.c_str(), _feature.size(), _delim, _quote_char.c_str());
    }

    return _tags.size();
  }

  const std::string tag(int idx) const {
    if (idx < n_tags()) {
      return _tags[idx];
    } else {
      return std::string();
    }
  }

  const std::string str() const {
    return _surface + _separator + _feature;
  }

 private:
  std::string _surface;
  std::string _feature; // comma separated string
  double _chunk_start_probability{0.0};
  mutable std::vector<std::string> _tags;
  std::string _separator{SURFACE_END};
  char _delim = FEATURE_SEP;
  std::string _quote_char{"\""};
};

struct PyChunk
{
  int id{0};
  int head_id{-1};
  int head_id_gold{-1};
  int head_id_cand{-1};
  double depend_prob{0.0f};
  char depend_type_gold{'D'};
  char depend_type_cand{'D'};

  void set_dependents(const std::vector<PyChunk> &chunks) {
    _dependents = chunks;
  }

  void set_tokens(const std::vector<PyToken> &tokens) {
    _tokens = tokens;
  }

  void set_tokens(std::vector<PyToken> &&tokens) {
    _tokens = tokens;
  }

  const std::vector<PyChunk> &dependents() const {
    return _dependents;
  }

  const std::vector<PyToken> &tokens() const {
    return _tokens;
  }

  // Chunk(Bunsetsu) string
  // Simply concatenate token surfaces.
  const std::string str() const {
    std::string s;
    for (const auto &tok : _tokens) {
      s += tok.surface();
    }
    return s;
  }

  const std::string print(bool prob = false) const {
    std::stringstream ss;

    ss << "* " << id << " " << head_id << "D";
    if (prob) {
      ss << "@" << depend_prob;
    }
    ss << "\n";

    for (const auto &tok : _tokens) {
      ss << tok.surface() << "\t" << tok.feature() << "\n";
    }

    return ss.str();
  }

  std::vector<PyChunk> _dependents;
  std::vector<PyToken> _tokens;
};

class PySentence
{
 public:
  PySentence() = default;

  const std::string &str() const {
    return _str;
  }

  void set_str(const std::string &s) {
    _str = s;
  }

  const std::vector<PyToken> tokens() const {
    return _tokens;
  }

  const std::vector<PyChunk> chunks() const {
    return _chunks;
  }

  void set_chunks(const std::vector<PyChunk> &rhs) {
    _chunks = rhs;
  }

  void set_chunks(std::vector<PyChunk> &&rhs) {
    _chunks = rhs;
  }

  const std::string print(bool prob = false) const {
    std::stringstream ss;

    for (const auto &chunk : _chunks) {
      ss << chunk.print(prob);
    }

    ss << "EOS\n";

    return ss.str();
  }

 private:
  std::string _str;
  std::vector<PyToken> _tokens;
  std::vector<PyChunk> _chunks;
};

class PyJdepp {
 public:
  const uint32_t kMaxChars = IOBUF_SIZE / 4; // Usually UTF-8 Japanese character is 3-bytes. div by 4 is convervative estimate.
  PyJdepp() {}
  PyJdepp(const std::string &model_path)
      : _model_path(model_path)  {
    load_model(_model_path);
  }

  void set_threads(uint32_t nthreads) {
    _nthreads = nthreads;
  }

  bool load_model(const std::string &model_path) {

    //
    // Curently we must set all parameters in options as (argc, argv), then construct parser.
    //

    //std::cout << "model_path " << model_path << "\n";
    _argv_str.push_back("pyjdepp");
    //_argv_str.push_back("--verbose");
    //_argv_str.push_back("10");
    _argv_str.push_back("-m");
    _argv_str.push_back(model_path);

    // TODO: args for learner, chunker, depend

    setup_argv();

    pdep::option options(int(_argv.size()), _argv.data(),
      int(_learner_argv.size()), _learner_argv.data(),
      int(_chunk_argv.size()), _chunk_argv.data(),
      int(_depend_argv.size()), _depend_argv.data()
    );

    if (_parser && _parser->model_loaded()) {
      // discard previous model&instance.
      delete _parser;
    }

    _parser = new pdep::parser(options);
    if (!_parser->load_model()) { // use setting in argv for model path
      py::print("Model load failed:", model_path);
    }

    return _parser->model_loaded();
  }

  PySentence parse_from_postagged(const std::string &input_postagged) const {
    if (!_parser->model_loaded()) {
      py::print("Model is not yet loaded.");
      return PySentence();
    }

    if (input_postagged.size() > IOBUF_SIZE) {
      py::print("Input too large. Input bytes must be less than ", IOBUF_SIZE);
      return PySentence();
    }

    const pdep::sentence_t *sent = _parser->parse_from_postagged(input_postagged.c_str(), input_postagged.size());
    if (!sent) {
      py::print("Failed to parse text from POS tagged string");
      return PySentence();
    }

    PySentence pysent;

    // We create copy for each chunk/token.
    // This approach is redundunt and not memory-efficient,
    // but this make Python binding easier(we don't need to consider lifetime of Python/C++ object)
    const char *str = sent->print_tostr(pdep::RAW, /* print_prob */false);

    if (!str) {
      py::print("Failed to get string from C++ sentence_t struct.");
      return PySentence();
    }

    // Assume single sentence in input text(i.e. one `EOS` line)
    std::string header = "# S-ID: " + std::to_string(1) + "; J.DepP\n";
    pysent.set_str(header + std::string(str));

    std::vector<PyChunk> py_chunks;

    const std::vector<const pdep::chunk_t *> chunks = sent->chunks();
    for (size_t i = 0; i < chunks.size(); i++) {
      const pdep::chunk_t *b = chunks[i];

      const std::vector<const pdep::chunk_t *> deps = b->dependents();

      std::vector<PyChunk> py_deps;

      for (size_t k = 0; k < deps.size(); k++) {
        PyChunk d;
        d.id = deps[k]->id;
        d.head_id = deps[k]->head_id;
        d.head_id_gold = deps[k]->head_id_gold;
        d.head_id_cand = deps[k]->head_id_cand;
        d.depend_prob = deps[k]->depnd_prob;
        d.depend_type_gold = deps[k]->depnd_type_gold;
        d.depend_type_cand = deps[k]->depnd_type_cand;

        // Create copy of token info.
        std::vector<PyToken> toks;
        for (const pdep::token_t *m = deps[k]->mzero(); m <= deps[k]->mlast(); m++) {
          std::string surface(m->surface, m->length);
          PyToken py_tok(surface, m->feature, m->chunk_start_prob);

          toks.push_back(py_tok);
        }
        d.set_tokens(toks);

        py_deps.push_back(d);
      }

      PyChunk py_chunk;

      py_chunk.id = b->id;
      py_chunk.head_id = b->head_id;
      py_chunk.head_id_gold = b->head_id_gold;
      py_chunk.head_id_cand = b->head_id_cand;
      py_chunk.depend_prob = b->depnd_prob;
      py_chunk.depend_type_gold = b->depnd_type_gold;
      py_chunk.depend_type_cand = b->depnd_type_cand;

      py_chunk.set_dependents(py_deps);

      std::vector<PyToken> toks;
      for (const pdep::token_t *m = b->mzero(); m <= b->mlast(); m++) {
        std::string surface(m->surface, m->length);
        PyToken py_tok(surface, m->feature, m->chunk_start_prob);

        toks.push_back(py_tok);
      }
      py_chunk.set_tokens(toks);


      py_chunks.push_back(py_chunk);
    }

    pysent.set_chunks(std::move(py_chunks));

    return pysent;
  }

  std::vector<PySentence> parse_from_postagged_batch(const std::vector<std::string> &input_postagged_array) const {
    std::vector<PySentence> sents;

    // NOTE: threading is not supported in J.DepP(internal state is get corrupted in theaded execution)
    // Use serial execution for a while.
    sents.resize(input_postagged_array.size());

    for (size_t k = 0; k < input_postagged_array.size(); k++) {
      sents[k] = parse_from_postagged(input_postagged_array[k]);
    }

    return sents;

  }

  bool model_loaded() const {
    return (_parser && _parser->model_loaded());
  }

  // for internal debugging
  void run() {
    _parser->run();
  }

 private:
  void setup_argv() {
    _argv.clear();
    for (auto &v : _argv_str) {
      _argv.push_back(const_cast<char *>(v.c_str()));
    }
    _argv.push_back(nullptr); // must add 'nullptr' at the end, otherwise out-of-bounds access will happen in optparse

    for (auto &v : _learner_argv_str) {
      _learner_argv.push_back(const_cast<char *>(v.c_str()));
    }
    _learner_argv.push_back(nullptr);

    for (auto &v : _chunk_argv_str) {
      _chunk_argv.push_back(const_cast<char *>(v.c_str()));
    }
    _chunk_argv.push_back(nullptr);

    for (auto &v : _depend_argv_str) {
      _depend_argv.push_back(const_cast<char *>(v.c_str()));
    }
    _depend_argv.push_back(nullptr);
  }

  uint32_t _nthreads{0};  // 0 = use all cores
  std::string _model_path;
  pdep::parser *_parser{nullptr};

  std::vector<char *> _argv;
  std::vector<std::string> _argv_str;

  std::vector<char *> _learner_argv;
  std::vector<std::string> _learner_argv_str;

  std::vector<char *> _chunk_argv;
  std::vector<std::string> _chunk_argv_str;

  std::vector<char *> _depend_argv;
  std::vector<std::string> _depend_argv_str;

};

}  // namespace pyjdepp

PYBIND11_MODULE(jdepp_ext, m) {
  m.doc() = "Python binding for Jdepp.";

  // Add Ext prefix to avoid name conflict of 'Jdepp' class in Python
  // world(defined in `jdepp/__init__.py`)
  py::class_<pyjdepp::PyJdepp>(m, "JdeppExt")
      .def(py::init<>())
      //.def(py::init<std::string>())
      .def_readonly("MAX_CHARS", &pyjdepp::PyJdepp::kMaxChars)
      .def("load_model", &pyjdepp::PyJdepp::load_model)
      .def("parse_from_postagged_batch", &pyjdepp::PyJdepp::parse_from_postagged_batch)
      .def("parse_from_postagged", [](const pyjdepp::PyJdepp &self, const std::string &str) {
        //if (!self.model_loaded()) {
        //  py::print("model not loaded.");
        //  return py::object(py::cast(nullptr)); // None
        //}
        return self.parse_from_postagged(str);
      })
      //.def("run", &pyjdepp::PyJdepp::run)
      ;

  py::class_<pyjdepp::PySentence>(m, "PySentence")
      .def(py::init<>())
      .def("str", &pyjdepp::PySentence::str)
      .def("tokens", &pyjdepp::PySentence::tokens)
      .def("chunks", &pyjdepp::PySentence::chunks)
      .def("print", &pyjdepp::PySentence::print, "Print parsed sentence in string", py::arg("prob") = false)
      .def("__repr__", &pyjdepp::PySentence::str)
      ;

  py::class_<pyjdepp::PyToken>(m, "PyToken")
      .def(py::init<>())
      .def("surface", &pyjdepp::PyToken::surface)
      .def("feature", &pyjdepp::PyToken::feature)
      .def("n_tags", &pyjdepp::PyToken::n_tags)
      .def("tag", &pyjdepp::PyToken::tag)
      .def("str", &pyjdepp::PyToken::str)
      .def("__repr__", &pyjdepp::PyToken::str)
      ;

  py::class_<pyjdepp::PyChunk>(m, "PyChunk")
      .def(py::init<>())
      .def_readonly("id", &pyjdepp::PyChunk::id)
      .def_readonly("head_id", &pyjdepp::PyChunk::head_id)
      .def_readonly("head_id_cand", &pyjdepp::PyChunk::head_id_cand)
      .def_readonly("head_id_gold", &pyjdepp::PyChunk::head_id_gold)
      .def_readonly("depend_prob", &pyjdepp::PyChunk::depend_prob)
      .def("dependents", &pyjdepp::PyChunk::dependents)
      .def("tokens", &pyjdepp::PyChunk::tokens)
      .def("str", &pyjdepp::PyChunk::str)
      .def("print", &pyjdepp::PyChunk::print, "Print chunk(bunsetsu) string", py::arg("prob") = false)
      .def("__repr__", [](const pyjdepp::PyChunk &self) { return self.print(); })
      ;
}
