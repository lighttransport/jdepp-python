// pecco -- please enjoy classification with conjunctive features
//  $Id: typedef.h 1875 2015-01-29 11:47:47Z ynaga $
// Copyright (c) 2008-2015 Naoki Yoshinaga <ynaga@tkl.iis.u-tokyo.ac.jp>
#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <sys/stat.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <limits>
#include <string>
#include <vector>
#include <map>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unordered_map>

#include "io-util.hh"

#ifndef USE_CEDAR
#define USE_CEDAR 1
#endif

#ifndef USE_KERNEL
#define USE_KERNEL 1
#endif

// Mecab format
#ifndef USE_MECAB
#define USE_MECAB 1
#endif

#ifndef USE_ARRAY_TRIE
#define USE_ARRAY_TRIE 1
#endif

#ifndef USE_PRUNING
#define USE_PRUNING 1
#endif


#if defined (USE_DARTS) || defined (USE_DARTS_CLONE)
#include <darts.h>
#endif
#ifdef USE_CEDAR
#define USE_REDUCED_TRIE 1
#include "cedar.h"
#endif

#define _STR_(x)   #x
#define _STR(x)    _STR_(x)
#define HERE       __FILE__ " [" _STR(__LINE__) "]: "

#ifdef USE_PROFILING
#define PROFILE(e) e
#else
#define PROFILE(e) ((void)0)
#endif

// NOTE: empty va args is not allowed for MSVC
#define my_errx(retcode, fmt, ...) do { \
  fprintf(stderr, "jdepp: "); \
  fprintf(stderr, "%s:%d:%s: ", __FILE__, __LINE__, __func__); \
  fprintf(stderr, fmt, __VA_ARGS__); \
  fprintf(stderr, "\n");\
  exit(retcode); \
} while(0)

#define my_warnx(fmt, ...) do { \
  fprintf(stdout, "jdepp: "); \
  fprintf(stdout, "%s:%d:%s: ", __FILE__, __LINE__, __func__); \
  fprintf(stdout, fmt, __VA_ARGS__); \
  fprintf(stdout, "\n");\
} while(0)

namespace ny {
  // type alias
  // ABUSE_TRIE requires 32-bit fl_t
#ifdef USE_FLOAT
  typedef float  fl_t;
#else
  typedef double fl_t;
#endif
  typedef unsigned int   uint;
  typedef unsigned char  uchar;
#ifdef USE_CEDAR
  typedef cedar::da <int, -1, -2, false, 32> trie;
#else
  typedef Darts::DoubleArray trie;
#endif
  typedef std::vector <uint>   fv_t;
  typedef fv_t::const_iterator fv_it;
  // template typedef
#ifdef USE_HASH
  template <typename _Key, typename _Tp>
#ifdef USE_TR1_HASH
  struct map { typedef std::tr1::unordered_map <_Key, _Tp> type; };
#else
  struct map { typedef std::unordered_map <_Key, _Tp> type; };
#endif
#else
  template <typename _Key, typename _Tp>
  struct map { typedef std::map <_Key, _Tp> type; };
#endif
  template <typename T>
  struct counter { typedef std::vector <std::pair <ny::uint, T> > type; };
  // uncopyable mix-in
  class Uncopyable {
  protected:
    Uncopyable() {}
    ~Uncopyable() {}
  private:
    Uncopyable (const Uncopyable&);
    Uncopyable& operator= (const Uncopyable&);
  };
  // less for pointers
  template <typename T>
  struct pless {
    bool operator () (const T* a, const T* b) const {
      while (1) {
        if      (*a == '\0') return (*b != '\0');
        else if (*b == '\0') return false;
        else if (static_cast <uint> (*a) > static_cast <uint> (*b))
          return false;
        else if (static_cast <uint> (*a) < static_cast <uint> (*b))
          return true;
        ++a; ++b;
      }
    }
  };
  // double array keys; key must be terminated with '\0'
  template <typename T, typename U>
  class TrieKeyBase : private Uncopyable {
  public:
    T*     key;
    U*     cont;
    size_t len;
    TrieKeyBase () : key (0), cont (0), len (0) {}
    TrieKeyBase (T* k, U* c, size_t l = 0, size_t nl = 1)
      : key (0), cont (0), len (l ? l : length (k)) {
      key = new T[len + 1];
      key[len] = '\0';
      std::copy (k, k + len, key);
      cont = new U[nl];
      if (c) std::copy (c, c + nl, cont);
    }
    size_t length (const T* k) const {
      const T* p = k;
      while (*p != static_cast<T> (0)) ++p;
      return static_cast <size_t> (p - k);
    }
    bool operator< (const TrieKeyBase& a) const
    { return pless <T> () (key, a.key); }
    ~TrieKeyBase () {
      if (key)  delete [] key;
      if (cont) delete [] cont;
    }
  };
  //
  template <typename T, typename U>
  struct TrieKeypLess {
    typedef TrieKeyBase <T, U> Key;
    bool operator () (const Key* a, const Key* b) const
    { return pless <T> () (a->key, b->key); }
  };
#if 1
  // getline wrapper
  static inline bool getLine (FILE*& fp, char*& line, size_t& read) {
#if 0
#ifdef __APPLE__
    if ((line = fgetln (fp, &read)) == NULL) return false;
#else
    static ssize_t read_ = 0; static size_t size = 0; // static for inline
    if ((read_ = getline (&line, &size, fp)) == -1) return false;
    read = read_;
#endif

#else
    size_t read_ = 0; size_t size = 0;
    read_ = ioutil::my_getline(fp, &line, &size);
    if (read_ == size_t(-1)) {
      return false;
    }
    read = read_;
#endif
    *(line + read - 1) = '\0';
    return true;
  }
#endif
}
#endif /* TYPEDEF_H */
