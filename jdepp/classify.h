// pecco -- please enjoy classification with conjunctive features
//  $Id: classify.h 1875 2015-01-29 11:47:47Z ynaga $
// Copyright (c) 2008-2015 Naoki Yoshinaga <ynaga@tkl.iis.u-tokyo.ac.jp>
//
// Modification by Light Tansport Entertainment Inc.
//
#ifndef CLASSIFY_H
#define CLASSIFY_H

//#include <sys/stat.h>
//#include <getopt.h>
//#include <err.h>
#include <cmath>
#include <cassert>
#include <climits>
#include <vector>
#include <sstream>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include "typedef.h"
#include "timer.h"

#ifndef DEFAULT_CLASSIFIER
#define DEFAULT_CLASSIFIER FST
#endif

// Use linear & kernel
#ifndef USE_LINEAR
#define USE_LINEAR
#endif

#ifndef USE_KERNEL
#define USE_KERNEL
#endif

#ifndef TRIE_SUFFIX
#define TRIE_SUFFIX "cda"
#endif

#define PECCO_COPYRIGHT  "pecco - please enjoy classification w/ conjunctive features\n\
Copyright (c) 2008-2015 Naoki Yoshinaga, All rights reserved.\n\
\n\
Usage: %s [options] model [test]\n\
\n\
model   model file             model\n\
test    test file              test examples; read STDIN if omitted\n\
\n"

#define PECCO_OPT0 "Optional parameters:\n\
  -t, --classifier=TYPE        select classifier type\n\
                                 0 - PKI (kernel model only)\n"

#if   defined (USE_PKE_AS_DEFAULT)
#define PECCO_OPT1 "                               * 1 - PKE | SPLIT\n\
                                 2 - FST\n"
#elif defined (USE_FST_AS_DEFAULT)
#define PECCO_OPT1 "                                 1 - PKE | SPLIT\n\
                               * 2 - FST\n"
#else
#define PECCO_OPT1 "                                 1 - PKE | SPLIT\n\
                                 2 - FST\n"
#endif

#ifdef USE_CEDAR
#if defined (USE_PMT_AS_DEFAULT)
#define PECCO_OPT2 "                               * 3 - PMT\n\
  -p, --pmsize=INT             set upper limit of partial margins (1 << INT; default: 20)\n"
#else
#define PECCO_OPT2 "                                 3 - PMT\n\
  -p, --pmsize=INT             set upper limit of partial margins (1 << INT; default: 20)\n"
#endif
#else
#define PECCO_OPT2
#endif

#define PECCO_OPT3 "  -e, --event=FILE             examples to obtain feature count for reordering ("")\n\
  -f, --fst-event=FILE         examples to enumerate feature sequence in FST ("")\n\
  -i, --fst-prune-factor=INT   use FST with 2^-INT of feature sequences (0)\n\
  -b, --fst-build-verbose      build FSTs with 2^-i (i > 0) of feature sequences\n\
  -c, --force-compile          force to recompile model\n\
  -O, --output=TYPE            select output type of testing\n\
                               * 0 - report accuracy\n\
                                 1 - + labels\n\
                                 2 - + labels + classifier outputs\n\
                                 3 - + labels + probabilities\n\
  -o, --output!=TYPE           output examples with labels/margins\n\
\n"

#define PECCO_OPT_KERNEL "Optional parameters in kernel model:\n\
  -m, --pke-minsup=INT         threshold to feature frequency (1)\n\
  -s, --pke-sigma=FLOAT        threshold to feature weight (0)\n\
  -r, --split-ratio=FLOAT      threshold to feature frequency ratio (0)\n\
\n"

#define PECCO_OPT_MISC "Misc.:\n\
  -v, --verbose=INT            verbosity level (0)\n\
  -h, --help                   show this help and exit\n"

#if 0
static const  char* pecco_short_options = "t:e:p:f:i:bcO:o:m:s:r:v:h";
static struct option pecco_long_options[] = {
  {"classifier type",   required_argument, NULL, 't'},
  {"event",             required_argument, NULL, 'e'},
  {"pmt-size",          required_argument, NULL, 'p'},
  {"fst-event",         required_argument, NULL, 'f'},
  {"fst-prune_ratio",   required_argument, NULL, 'i'},
  {"fst-build-verbose", no_argument,       NULL, 'b'},
  {"force-compile",     no_argument,       NULL, 'c'},
  {"output",            required_argument, NULL, 'O'},
  {"output!",           required_argument, NULL, 'o'},
  {"pke-minsup",        required_argument, NULL, 'm'},
  {"pke-sigma",         required_argument, NULL, 's'},
  {"split-ratio",       required_argument, NULL, 'r'},
  {"verbose",           required_argument, NULL, 'v'},
  {"help",              no_argument,       NULL, 'h'},
  {NULL, 0, NULL, 0}
};

extern char* optarg;
extern int    optind;
#endif

namespace pecco {

  enum model_t  { KERNEL, LINEAR };
#ifdef USE_CEDAR
  enum algo_t   { PKI, PKE, FST, PMT };
#else
  enum algo_t   { PKI, PKE, FST };
#endif
  enum binary_t { BINARY = true, MULTI = false };
  enum output_t { NONE = 0, LABEL = 1, SCORE = 2, PROB = 3 };
  // str to numerical wrapper
  template <typename T> T strton (const char* s, char** error);
  template <typename T> T strton (const char* s);
  // options
  struct option { // option handler
    const char* com, *train, *test, *model, *event;
    //
    const char*  minsup;
    const char*  sigma;
    const char*  fratio;
    model_t      type;    // model-type
    algo_t       algo;
    int          output;
    uint8_t      fst_factor;
    bool         fst_verbose;
    bool         force;
    uint8_t      verbose;
    size_t       pmsize;
    option () : com ("--"), train (0), test (0), model (0), event (""), minsup ("1"), sigma ("0"), fratio ("0"), type (KERNEL), algo (DEFAULT_CLASSIFIER),
                output (0), fst_factor (0), fst_verbose (false), force (false), verbose (0), pmsize (20)  {}
#if 0
    option (int argc, char ** argv) : com (argc ? argv[0] : "--"), train (0), test (0), model (""), event (""), minsup ("1"), sigma ("0"), fratio ("0"), type (KERNEL), algo (DEFAULT_CLASSIFIER), output (NONE),  fst_factor (0), fst_verbose (false), force (false), verbose (0), pmsize (20) {
      set (argc, argv);
    }
    void set (int argc, char ** argv) { // getOpt
      if (argc == 0) return;
      optind = 1;
      size_t minsup_ = 1;
      double sigma_ (0.0), fratio_ (0.0);
      while (1) {
        int opt = getopt_long (argc, argv,
                               pecco_short_options, pecco_long_options, NULL);
        if (opt == -1) break;
        char* err = NULL;
        switch (opt) {
          case 't': algo   = strton <algo_t>   (optarg, &err); break;
          case 'o': output = 0x100;
          case 'O': output |= strton <int> (optarg, &err); break;
          case 'e': train  = optarg; break;
          case 'f': event  = optarg; break;
          case 'm': minsup = optarg; minsup_ = strton <size_t> (optarg, &err); break;
#ifdef USE_FLOAT
          case 's': sigma  = optarg; sigma_  = std::strtod (optarg, &err); break;
          case 'r': fratio = optarg; fratio_ = std::strtod (optarg, &err); break;
#else
          case 's': sigma  = optarg; sigma_  = std::strtod (optarg, &err); break;
          case 'r': fratio = optarg; fratio_ = std::strtod (optarg, &err); break;
#endif
          case 'p': pmsize = strton <size_t>  (optarg, &err); break;
          case 'i': fst_factor  = strton <uint8_t> (optarg, &err); break;
          case 'b': fst_verbose = true; break;
          case 'c': force       = true; break;
          case 'v': verbose     = strton <uint8_t> (optarg, &err); break;
          case 'h': printCredit (); printHelp (); std::exit (0);
          default:  printCredit (); std::exit (0);
        }
        if (err && *err)
          errx (1, HERE "unrecognized option value: %s", optarg);
      }
      // errors & warnings
#ifdef USE_CEDAR
      if (algo != PKE && algo != FST && algo != PMT && algo != PKI)
#else
      if (algo != PKE && algo != FST && algo != PKI)
#endif
        errx (1, HERE "unknown classifier type [-t].");
      if (algo == PKI) {
        if (minsup_ != 1)
          errx (1, HERE "PKE minsup [-m] must be 0 in PKI [-t 0].");
        if (sigma_ != 0.0)
          errx (1, HERE "PKE simga [-s] must be 0 in PKI [-t 0].");
        if (fratio_ != 0.0)
          errx (1, HERE "SPLIT ratio [-r] must be 0 in PKI [-t 0].");
      }
      if (std::strcmp (argv[0], "--") == 0) return; // skip
      if (argc < optind + 1) {
        printCredit ();
        errx (1, HERE "Type `%s --help' for option details.", com);
      }
#ifndef USE_MODEL_SUFFIX
      if (fst_verbose)
        errx (1, HERE "[-b] building multiple FSTs are useless since model suffix disabled.");
#endif
      model = argv[optind];
      setType ();
      if (type == LINEAR) {
        if (algo == PKI)
          errx (1, HERE "PKI [-t 0] is not available for LLM.");
        if (minsup_ != 1)
          warnx ("WARNING: PKE minsup [-m] is ignored in LLM.");
        if (sigma_ != 0.0)
          warnx ("WARNING: PKE sigma [-s] is ignored in LLM.");
        if (fratio_ != 0.0)
          warnx ("WARNING: SPLIT ratio [-r] is ignored in LLM.");
      }
#ifdef USE_CEDAR
      if (algo == PMT) {
        if (pmsize <= 0)
          errx (1, HERE "PMT [-t 3] requires > 0 size [-p]");
      } else pmsize = 0;
#endif
      if (algo == FST && std::strcmp (event, "") == 0)
        errx (1, HERE "FST [-t 2] requires possible examples [-f];\n (you can use the training examples)");
      if (++optind != argc) test = argv[optind];
    }
#endif
    void setType () {
      FILE* fp = std::fopen (model, "r");
      if (! fp || std::feof (fp))
        my_errx (1, HERE "no model found: %s; train a model first", model);
      switch (std::fgetc (fp)) {
        case 'o': // opal;      delegate
        case 's': // LIBSVM;    delegate
        case 'S': // SVM-light; delegate
        case 'T': type = KERNEL;  break;
        default:  type = LINEAR;  break;
      }
      std::fclose (fp);
    }
    void printCredit ()
    { std::fprintf (stderr, PECCO_COPYRIGHT, com); }
    void printHelp ()
    { std::fprintf (stderr, PECCO_OPT0 PECCO_OPT1 PECCO_OPT2 PECCO_OPT3 PECCO_OPT_KERNEL PECCO_OPT_MISC); }
  };
#ifdef USE_LFU_PMT
  // memory-efficient implementation of stream-summary data structure
  struct elem_t {
    int prev;
    int next;
    int link; // link to bucket / item
    elem_t (const int prev_ = -1, const int next_ = -1, const int link_ = -1)
      : prev (prev_), next (next_), link (link_) {}
  };
  struct count_t : public elem_t {
    size_t count;
    count_t () : elem_t (), count (0) {}
  };
  template <typename T>
  class ring_t {
  protected:
    T* _data;
  public:
    int& prev (const int n) const { return _data[n].prev; }
    int& next (const int n) const { return _data[n].next; }
    int& link (const int n) const { return _data[n].link; }
    void pop  (const int n) const {
      next (prev (n)) = next (n);
      prev (next (n)) = prev (n);
    }
    void push (const int n, const int prev_) const
    { next (prev (n) = prev_) = prev (next (n) = next (prev_)) = n; }
    ring_t () : _data (0) {}
  };
  class item_t : public ring_t <elem_t> {
  public:
    bool is_singleton (const int n) const
    { return prev (n) == n && next (n) == n; }
    item_t (const size_t capacity) : ring_t () { _data  = new elem_t[capacity]; }
    ~item_t () { delete [] _data; }
  };
  class bucket_t : public ring_t <count_t> {
  public:
    size_t& count (const int n) const { return _data[n].count; }
    int add_elem  () {
      if (_size == _capacity) {
        _capacity += _capacity;
        void* tmp = std::realloc (_data, sizeof (count_t) * _capacity);
        if (! tmp)
          std::free (_data), errx (1, HERE "memory reallocation failed");
        _data = static_cast <count_t*> (tmp);
      }
      return _size++;
    }
    void pop_item  (item_t& _item, const int m, const int n) const {
      _item.pop (n);
      if (link (m) == n) link (m) = _item.next (n);
    }
    void push_item (item_t& _item, const int m, const int n) const  {
      _item.push (n, link (m));
      _item.link (n) = m;
    }
    void set_child (item_t& _item, const int m, const int n) const {
      _item.next (n) = _item.prev (n) = n; // singleton
      _item.link (n) = m;
    }
    bucket_t () : ring_t <count_t> (), _size (0), _capacity (1) {
      _data = static_cast <count_t*> (std::malloc (sizeof (count_t)));
      prev (0) = next (0) = 0;
    }
    ~bucket_t () { std::free (_data); }
  private:
    int _size;
    int _capacity;
  };
  // eco-friendly recycling plant for bucket objects
  class bucket_pool {
  public:
    void recycle_bucket (bucket_t& _bucket, const int m)
    { _bucket.next (m) = _M; _M = m; }
    int create_bucket (bucket_t& _bucket) {
      if (_M == -1) return _bucket.add_elem ();
      const int m = _M;
      _M = _bucket.next (_M);
      return m;
    }
    bucket_pool () : _M (-1) {}
    ~bucket_pool () {}
  private:
    int _M;
    bucket_pool (const bucket_pool&);
    bucket_pool& operator= (const bucket_pool&);
  };
#else
  struct elem_t {
    int prev;
    int next;
    elem_t (const int prev_ = -1, const int next_ = -1)
      : prev (prev_), next (next_) {}
  };
  class ring_t { // linked list
  private:
    elem_t* _data;
    int     _head;
    int     _nelm;
    int     _capacity;
  public:
    int& prev (const int n) const { return _data[n].prev; }
    int& next (const int n) const { return _data[n].next; }
    int  get_element () { // return least recently used
      int n = 0;
      if (_nelm == _capacity) { // full
        n = _head;
        _head = next (_head);
      } else {
        if (_nelm) {
          const int tail = prev (_head);
          prev (_nelm) = tail;
          next (_nelm) = _head;
          prev (_head) = next (tail) = _nelm;
        } else {
          _head = 0;
          prev (_head) = next (_head) = _head;
        }
        n = _nelm;
        ++_nelm;
      }
      return n;
    }
    void move_to_back (const int n) { // bad?
      if (n == _head) {
        _head = next (n);
      } else { // pop and move
        next (prev (n)) = next (n);
        prev (next (n)) = prev (n);
        int& tail = prev (_head);
        prev (n) = tail;
        next (n) = _head;
        tail = next (tail) = n;
      }
    }
    ~ring_t () { delete [] _data; }
    ring_t (int capacity) : _data (new elem_t[capacity]), _head (-1), _nelm (0), _capacity (capacity) {}
  };
#endif
  // \sum_{i=0}^{k} nCk * num_class is assigned to an array-based pseudo trie
#ifdef USE_ARRAY_TRIE
  static const size_t PSEUDO_TRIE_N[] = {0, 21, 11, 8, 6};
#else
  static const size_t PSEUDO_TRIE_N[] = {0, 0, 0, 0, 0};
#endif
  // type alias
  // uchar * -> fl_t  ; conj. feat. -> weight (float)
  typedef ny::TrieKeyBase  <ny::uchar, ny::fl_t> FeatKey;
  typedef ny::TrieKeypLess <ny::uchar, ny::fl_t> FeatKeypLess;
  // uchar * -> fl_t  ; feat. seq.  -> weight (float) or weight ID (int)
  struct FstKey : public FeatKey {
    size_t   weight; // used for node cutoff
    ny::uint count;  // used for node cutoff
    bool     leaf;
    FstKey () : FeatKey (), weight (0), count (0), leaf (false) {}
    FstKey (ny::uchar* k, ny::fl_t* c, size_t l, ny::uint nl)
      : FeatKey (k, c, l, nl), weight (0), count (0), leaf (false) {}
    bool is_prefix (FstKey *a) const { // whether key is a->key's prefix
      if (len > a->len) return false;
      for (size_t i = 0; i < len; ++i)
        if (key[i] != a->key[i]) return false;
      return true;
    }
  };
  // feature sequence ordering; see Yoshinaga and Kitsuregawa (EMNLP 2009)
  struct FstKeypLess {
    bool operator () (const FstKey* a, const FstKey* b) const {
      // keep frequent / long keys
      if (a->count * a->weight < b->count * b->weight)
        return true;
      else if (a->count * a->weight > b->count * b->weight)
        return false;
      // keep keys with frequent features
      else return FeatKeypLess () (b, a);
      // return (std::memcmp (a->key, b->key, a->len) > 0);
    }
  };
  // int <-> uint <-> float converter
  union byte_4 {
    int i;
    ny::uint u;
    float f;
    byte_4 (int n)   : i (n) {}
    byte_4 (float b) : f (b) {}
  };
  // build / save da at once
  static inline void build_trie (ny::trie* da,
                                 const std::string& name,
                                 const std::string& fn,
                                 std::vector <const char*>& str,
                                 const std::vector <size_t>& len,
                                 const std::vector <ny::trie::result_type>& val,
                                 bool flag, const char* mode = "wb") {
    if (flag) std::fprintf (stderr, " building %s..", name.c_str ());
    if (da->build (str.size (), &str[0], &len[0], &val[0]) != 0 ||
        da->save  (fn.c_str (), mode) != 0 ) {
      my_errx (1, HERE "failed to build %s trie.", name.c_str ());
    }
    if (flag) std::fprintf (stderr, "done.\n");
  }
  // check file refreshness
  static inline bool newer (const char* newer, const char* base) {
    struct stat newer_fs, base_fs;
    if (stat (newer, &newer_fs) == -1) return false;
    stat (base,  &base_fs); // need not exist
    return difftime (newer_fs.st_mtime, base_fs.st_mtime) >= 0;
  }
  // bytewise coding (cf. Williams & Zobel, 1999)
  static const size_t KEY_SIZE = 8; // >= log_{2^7} _nf + 1
  class byte_encoder {
  public:
    byte_encoder () : _len (0), _key () {}
    byte_encoder (ny::uint i) : _len (0), _key () { encode (i); }
    ny::uint encode (ny::uint i, ny::uchar* const key_) const {
      ny::uint len_ = 0;
      for (key_[len_] = (i & 0x7f); i >>= 7; key_[++len_] = (i & 0x7f))
        key_[len_] |= 0x80;
      return ++len_;
    }
    void encode (const ny::uint i) { _len = encode (i, _key); }
    ny::uint decode (ny::uint& i, const ny::uchar* const key_) const {
      ny::uint b (0), len_ (0);
      for (i = key_[0] & 0x7f; key_[len_] & 0x80; i += (key_[len_] & 0x7fu) << b)
        ++len_, b += 7;
      return ++len_;
    }
    ny::uint    len () { return _len; }
    const char* key () { return reinterpret_cast <const char *> (&_key[0]); }
  private:
    ny::uint  _len;
    ny::uchar _key[KEY_SIZE];
  };
  struct pn_t {
    ny::fl_t pos;
    ny::fl_t neg;
    pn_t () : pos (0), neg (0) {}
    bool operator== (const pn_t& pn) const {
      return
        std::fpclassify (pos - pn.pos) == FP_ZERO &&
        std::fpclassify (neg - pn.neg) == FP_ZERO;
    }
  };
  //
  static const ny::uint INSERT_THRESH = 10;
  static const ny::uint NBIT = 5;
  static const ny::uint NBIN = 1 << NBIT;
  template <typename T>
  class sorter_t {
  public:
    typedef typename T::iterator Iterator;
    typedef typename std::iterator_traits <Iterator>::value_type value_type;
    sorter_t  () : _temp () {}
    ~sorter_t () {}
    // for short feature vector < 50
    void insertion_sort (const Iterator& first, const Iterator& last) {
      std::less <value_type> cmp;
      if (std::distance (first, last) <= 1) return;
      Iterator sorted = first;
      for (++sorted; sorted != last; ++sorted) {
        value_type temp = *sorted;
        Iterator prev, curr;
        prev = curr = sorted;
        for (--prev; curr != first && cmp (temp, *prev); --prev, --curr)
          *curr = *prev;
        *curr = temp;
      }
    }
    // MSD radix sort
    void radix_sort (const Iterator& first, const Iterator& last, const ny::uint shift) {
      const size_t len = static_cast <size_t> (std::distance (first, last));
      if (len <= 1) return;
      if (len < INSERT_THRESH * ((shift / NBIT) + 1)) { // may depend on depth
        insertion_sort (first, last);
      } else {
        // do radix sort
        if (_temp.size () < len) _temp.resize (len);
        int count[NBIN + 1];
        std::fill (&count[0], &count[NBIN + 1], 0);
        for (Iterator it = first; it != last; ++it)
          ++count[(*it >> shift) & (NBIN - 1)];
        for (size_t i = 1; i <= NBIN; ++i) count[i] += count[i - 1];
        for (Iterator it = first; it != last; ++it)
          _temp[static_cast <size_t> (--count[(*it >> shift) & (NBIN - 1)])] = *it;
        std::copy (&_temp[0], &_temp[len], first);
        if (! shift) return;
        for (size_t i = 0; i < NBIN; ++i)
          if (count[i + 1] - count[i] >= 2)
            radix_sort (first + count[i], first + count[i + 1], shift - NBIT);
      }
    }
    // Zipf-aware sorting funciton
    void bucket_sort (const Iterator& first, const Iterator& last, const ny::uint shift) {
      if (std::distance (first, last) <= 1) return;
      uint64_t bucket = 0;
      // assuming most feature IDs are less than sizeof (U) * 8
      Iterator it (last);
      for (Iterator jt = it; it != first; ) {
        if (*--it < sizeof (uint64_t) * 8)
          bucket |= (static_cast <uint64_t> (1) << *it); // bug in 32bit
        else // leave unsorted (large) feature IDs on tail
          *--jt = *it;
      }
      // do bucket sort
      // pick input numbers by twiddling bits
      //   cf. http://www-cs-faculty.stanford.edu/~uno/fasc1a.ps.gz (p. 8)
      while (bucket) {
        // *it = __builtin_ctzl (bucket);
        pecco::byte_4 b (static_cast <float> (bucket & - bucket)); // pull rightmost 1
        *it = (b.u >> 23) - 127; ++it; // pick it via floating number
        bucket &= (bucket - 1); // unset rightmost 1
      }
      // do radix sort for the tail
      radix_sort (it, last, shift); // for O(n) guarantee
    }
  private:
    std::vector <value_type> _temp;
  };
#ifdef USE_CEDAR
  typedef cedar::da <int, -1, -2, false>  pmtrie_t;
#endif
  template <typename T>
  class ClassifierBase : private ny::Uncopyable {
  protected:
    static const ny::uint MAX_KERNEL_DEGREE = 4;
    // type alias
    typedef ny::map <ny::uint, ny::uint>::type counter_t;
    typedef std::map <char *, ny::uint, ny::pless <char> > lmap; // unordered_map
    // options and classifier parameters
    const option           _opt;
    std::vector <double>   _score;
    ny::fv_t               _fv;
    sorter_t <ny::fv_t>    _sorter;
    ny::uint               _d;        // degree of feature combination
    ny::uint               _nl;       // # of labels
    // various sizes
    ny::uint               _nf;       // # features
    ny::uint               _nfbit;    // # features
    ny::uint               _nf_cut;   // # features (pruned)
    ny::uint               _ncf;      // # conjunctive features
    ny::uint               _nt;       // # training sample
    // for mapping label to label id
    ny::uint               _tli;      // default target label id
    std::vector <char*>    _li2l;     // label id -> label
    lmap                   _l2li;     // label -> label id
    // for mapping feature numbers to feature indices
    std::vector <ny::uint> _fn2fi;    // feature number -> feature index
    std::vector <ny::uint> _fi2fn;    // feature index  -> feature number
    counter_t              _fncnt;    // feature number -> count
    // double arrays for conjunctive features and fstrie
    ny::trie               _ftrie;    // conjunctive feature -> weight / weight_id
    ny::trie               _fstrie;   // feature sequece     -> weight / weight_id
#ifdef USE_CEDAR
    pmtrie_t               _pmtrie;   // feature sequece     -> weight / weight_id
    class func {
    public:
      int* const leaf;
      const int  size;
      void operator () (const int to_, const int to) {
        size_t from (static_cast <size_t> (to_)),  p (0);
        const int n = _trie->traverse ("", from, p, 0);
        if (n != pmtrie_t::CEDAR_NO_VALUE) leaf[n - 1] = to;
      }
      func (const int size_, pmtrie_t* const trie) : leaf (new int[size_]), size (size_), _trie (trie)
      { for (int i = 0; i < size; ++i) leaf[i] = 0; }
      ~func () { delete [] leaf; }
    private:
      pmtrie_t* _trie;
    } _pmfunc;
    int                    _pmi;
    double*                _pms;
#ifdef USE_LFU_PMT
    bucket_pool            _bucket_pool;
    bucket_t               _bucket;
    item_t                 _item;
#else
    ring_t                 _ring;
#endif
#endif
    // pruning when exact margin is unnecessary
    pn_t*                  _f2dpn;
    pn_t*                  _f2nf;
    std::vector <size_t>   _dpolyk;
    std::vector <pn_t>     _bound;
    // temporary variables
    ny::fl_t*              _fw;       // conjunctive feature id -> weight
    ny::fl_t*              _fsw;      // feature sequence id -> weight
    // timer
#ifdef USE_TIMER
    ny::TimerPool          _timer_pool;
    ny::Timer*             _enc_t;      // feature mapping
    ny::Timer*             _model_t;    // compiling/loading  model
    ny::Timer*             _classify_t; // pke classify
#endif
    // profiling
#ifdef USE_PROFILING
    unsigned long          _all;         // all       (for fst)
    unsigned long          _hit;         // hit ratio (for fst)
    unsigned long          _lookup;      // lookup    (for pke)
    unsigned long          _flen;        // traverse  (for pke)
    unsigned long          _traverse;    // traverse  (for pke)
    unsigned long          _lookup_sp;   // lookup    (for pke)
    unsigned long          _traverse_sp; // traverse  (for pke)
#endif
    T*        _derived ()             { return static_cast <T*>       (this); }
    const T*  _derived_const () const { return static_cast <const T*> (this); }

    bool isspace (const char p) const { return p == ' ' || p == '\t'; }

    // * a function that estimates the additional cost
    //   when eliminating nodes
    size_t _cost_fun (size_t m, size_t n) {
      // by frequency * # of features
      switch (_d) {
        case 1: return 0;
        case 2: return ((n*n - n) - (m*m - m)) / 2;
        case 3: return ((n*n*n - n) - (m*m*m - m)) / 6;
        case 4: return ((n*n*n*n - 2*n*n*n + 11*n*n - 10*n)
                        -(m*m*m*m - 2*m*m*m + 11*m*m - 10*m)) / 24;
        default: my_errx (1, "%s", "please add case statement."); return 0; // dummy;
      }
    }
    // bytewise coding for feature indexes (cf. Williams & Zobel, 1999)
    // various functions for sorting feature (index) vectors
    template <typename Iterator>
    static void _insertion_sort (const Iterator& first, const Iterator& last);
    template <typename Iterator>
    static void _radix_sort (const Iterator& first, const Iterator& last, const ny::uint shift);
    template <typename Iterator>
    static void _bucket_sort (const Iterator& first, const Iterator& last, const ny::uint shift);
    // feature mapping
    void _sortFv (ny::fv_t& fv) {
#ifdef SHIFT_LEFT_UNSIGNED_USES_SHL
      // for feature vectors following Zipf
      _sorter.bucket_sort (fv.begin (), fv.end (), _nfbit);
      // _sorter.radix_sort (fv.begin (), fv.end (), _nfbit);
      // _sorter.insertion_sort (fv.begin (), fv.end ());
      // std::sort (fv.begin (), fv.end ());
#else
      std::sort (fv.begin (), fv.end ());
#endif
    }

    void _convertFv2Fv (char* p, ny::fv_t& fv) const {
      // convert feature numbers (in string) into feature indices
      fv.clear ();
      while (*p) {
        const ny::uint fn = strton <ny::uint> (p, &p);
        if (fn < _fn2fi.size ())
          if (const ny::uint fi = _fn2fi[fn])
            fv.push_back (fi);
        ++p; while (*p && ! isspace (*p)) ++p; // skip value
        while (isspace (*p)) ++p; // skip (trailing) spaces
      }
    }

    void _convertFv2Fv (ny::fv_t& fv) const {
      // convert feature numbers to feature indices
      ny::fv_t::iterator jt = fv.begin ();
      for (ny::fv_it it = jt; it != fv.end (); ++it)
        if (*it < _fn2fi.size ())
          if (const ny::uint fi = _fn2fi[*it])
            *jt = fi, ++jt;
      fv.erase (jt, fv.end ());
    }

    bool _packingFeatures (std::vector <ny::fv_t>& fvv) {
      if (_opt.verbose > 0)
        std::fprintf (stderr, "packing feature id..");
      // reorder features according to their frequency in training data
      if (_opt.train) {
#if 1
        FILE* reader =  std::fopen (_opt.train, "r");
        if (! reader) my_errx (1, "no such file: %s", _opt.train);
        _nt         = 0;
        char*  line = 0;
        size_t read = 0;
        while (ny::getLine (reader, line, read)) {
          if (*line != '\0') {
            // assume primitive feature vectors
            char* p (line), *p_end (line + read - 1);
            while (p != p_end && ! isspace (*p)) {
              ++p;
            }
            ++p;
            while (p != p_end) {
              const ny::uint fn = strton <ny::uint> (p, &p);
              counter_t::iterator it = _fncnt.find (fn);
              if (it != _fncnt.end ()) ++it->second;
              ++p; while (p != p_end && ! isspace (*p)) ++p; // skip value
              while (isspace (*p)) ++p; // skip (trailing) spaces
            }
            ++_nt;
          }
        }
        std::fclose (reader);
#else
        my_errx(1, "%s", "Training is not supported in this build.");
#endif
      } else {
        // reorder features according to their frequency in support vectors / conjunctive features
        for (std::vector <ny::fv_t>::const_iterator it = fvv.begin ();
             it != fvv.end (); ++it) // for each unit
          for (ny::fv_it fit = it->begin (); fit != it->end (); ++fit) {
            counter_t::iterator jt = _fncnt.find (*fit); // for each feature
            if (jt != _fncnt.end ()) ++jt->second;
          }
      }
      // building feature mapping
      ny::counter <ny::uint>::type fn_counter;
      fn_counter.reserve (_fncnt.size ());
      ny::uint fn_max = 0;
      for (counter_t::const_iterator it = _fncnt.begin ();
           it != _fncnt.end (); ++it) {
        const ny::uint fn  = it->first;
        const ny::uint cnt = it->second;
        fn_max = std::max (fn, fn_max);
        fn_counter.push_back (ny::counter <ny::uint>::type::value_type (cnt, fn));
      }
      std::sort (fn_counter.begin (), fn_counter.end ());
      _fn2fi.resize (fn_max + 1, 0);
      _fi2fn.resize (_nf + 1, 0); // _nf
      ny::uint fi = 1;
      for (ny::counter <ny::uint>::type::reverse_iterator it = fn_counter.rbegin ();
           it != fn_counter.rend (); ++it) {
        const ny::uint fn = it->second;
        _fi2fn[fi] = fn;
        _fn2fi[fn] = fi;
        ++fi;
      }
      // rename features in conjunctive features / support vectors
      for (std::vector <ny::fv_t>::iterator it = fvv.begin ();
           it != fvv.end (); ++it) { // for each unit
        _convertFv2Fv (*it);
        _sortFv (*it); // for pkb; bug
      }
      if (_opt.verbose > 0)
        std::fprintf (stderr, "done.\n");
      return true;
    }

#if 1
    bool _setFStrie ();
#else
    // feature sequence trie builder
    bool _setFStrie () {
      const bool abuse = abuse_trie ();
      std::string fstrie_fn_ (std::string (_opt.event) + (abuse ? "." TRIE_SUFFIX : ".n" TRIE_SUFFIX));
      std::string fsw_fn_    (std::string (_opt.event) + ".weight");
      std::ostringstream ss;
#ifdef USE_MODEL_SUFFIX
      ss << ".-" << static_cast <uint16_t> (_opt.fst_factor);
#endif
      std::string fstrie_fn  (fstrie_fn_ + ss.str ());
      std::string fsw_fn     (fsw_fn_    + ss.str ());
      // check the update time of events
      if (_opt.verbose > 0)
        std::fprintf (stderr, "loading fstrie..");
      if (! _opt.force && newer (fstrie_fn.c_str (), _opt.event) &&
          _fstrie.open (fstrie_fn.c_str ()) == 0) {
        if (! abuse) { // load the pre-computed weights
          FILE* reader = std::fopen (fsw_fn.c_str (), "rb");
          if (! reader) my_errx (1, "no such file: %s", fsw_fn.c_str ());
          if (std::fseek (reader, 0, SEEK_END) != 0) return -1;
          const size_t nfs = static_cast <size_t> (std::ftell (reader)) / (_nl * sizeof (ny::fl_t));
          if (std::fseek (reader, 0, SEEK_SET) != 0) return -1;
          _fsw = new ny::fl_t [_nl * nfs];
          std::fread (&_fsw[0], sizeof (ny::fl_t), _nl * nfs, reader);
          std::fclose (reader);
        }
        if (_opt.verbose > 0) std::fprintf (stderr, "done.\n");
      } else {
        if (_opt.verbose > 0) std::fprintf (stderr, "not found.\n");
#if 1
        FILE* reader = std::fopen (_opt.event, "r");
        if (! reader)
          my_errx (1, "no such event file: %s", _opt.event);
        if (_opt.verbose > 0)
          std::fprintf (stderr, "building fstrie from %s..", _opt.event);
        unsigned long nt (0), nkey (0);
        std::set <FstKey*, FeatKeypLess> fst_key;
        FstKey q;
        std::vector <ny::uchar> s;
        std::vector <double> w (_nl, 0);
        char*  line = 0;
        size_t read = 0;
        for (ny::fv_t fv; ny::getLine (reader, line, read); fv.clear ()) {
          if (*line != '\0') {
            // skip label
            char* p (line), *p_end (line + read - 1);
            // skip label & space
            while (p != p_end && ! isspace (*p)) {
              ++p;
            }
            ++p;
            _convertFv2Fv (p, fv);
            _sortFv (fv);
            // precompute score
            if (KEY_SIZE * fv.size () >= s.size ())
              s.resize (KEY_SIZE * fv.size () + 1);
            ny::uint len (0), prev (0);
            byte_encoder encoder;
            for (ny::fv_t::iterator jt = fv.begin (); jt != fv.end (); ) {
              len += encoder.encode (*jt - prev, &s[len]);
              prev = *jt;
              s[len] = '\0'; q.key = &s[0]; q.len = len;
              std::set <FstKey *, FeatKeypLess>::iterator it
                = fst_key.lower_bound (&q);
              ++jt;
              if (it == fst_key.end () || FeatKeypLess () (&q, *it)) {
                ++nkey;
                std::fill (&w[0], &w[_nl], 0);
                if (is_binary_classification ())
                  _baseClassify <false, BINARY> (&w[0], jt - 1, fv.begin (), jt);
                else
                  _baseClassify <false, MULTI>  (&w[0], jt - 1, fv.begin (), jt);
                it = fst_key.insert (it, new FstKey (&s[0], 0, len, _nl));
                std::copy (&w[0], &w[_nl], (*it)->cont);
                const size_t j
                  = static_cast <size_t> (std::distance (fv.begin (), jt));
                (*it)->weight = _cost_fun (j - 1, j);
                if (jt == fv.end ()) (*it)->leaf = true;  // leaf (tentative)
              } else {
                if (jt != fv.end ()) (*it)->leaf = false; // this is not leaf
              }
              (*it)->count += 1;
            }
          }
          if (++nt % 1000 == 0 && _opt.verbose > 0)
            std::fprintf (stderr, "\r processing %ld events => %ld feature sequences",
                          nt, nkey);
        }
        q.key = 0; q.len = 0;
        std::fclose (reader);
        if (_opt.verbose > 0)
          std::fprintf (stderr, "\r processing %ld events => %ld feature sequences\n",
                        nt, nkey);
        std::set <FstKey*, FstKeypLess> fst_leaf;
        for (std::set <FstKey*, FstKeypLess>::const_iterator it = fst_key.begin ();
             it != fst_key.end (); ++it)
          if ((*it)->leaf) fst_leaf.insert (*it);
        if (_opt.verbose > 0)
          std::fprintf (stderr, " # leaf: %ld\n", fst_leaf.size ());
        std::vector <const char *> str;
        std::vector <size_t>       len;
        std::vector <int>          val;
        str.reserve (nkey);
        len.reserve (nkey);
        val.reserve (nkey);
        if (! abuse) _fsw = new ny::fl_t [_nl * nkey];
        size_t nkey_small = nkey;
        for (size_t j = 0; nkey_small > 0; ++j) {
          // sort dictionary order of key strings
          while (fst_key.size () > nkey_small) {
            // should memory release
            FstKey* const p = * fst_leaf.begin ();
            std::set <FstKey *, FeatKeypLess>::iterator it = fst_key.find (p);
            // WARNING: a leaf node is sorted by its impact; look left/right
            // add its longest prefix if there are no sibling
            // 1 2 3     l
            // 1 2 3 4   p
            // 1 2 3 5   r
            // 1 2 3 5 6
            if (it != fst_key.begin ()) { // not a root node
              FstKey* const l = *(--it); ++it;
              if (l->is_prefix (p)) { // prefix (next leaf)
                ++it;
                if (it == fst_key.end () || ! l->is_prefix (*it)) // no sibling
                   fst_leaf.insert (l);
                --it;
              }
            }
            fst_leaf.erase (p);
            fst_key.erase (it);
            delete p;
          }
          ny::uint i = 0;
          std::set <FstKey*, FeatKeypLess>::const_iterator it = fst_key.begin ();
          typedef std::map <std::vector <double>, size_t> w2id_t;
          w2id_t w2id;
          size_t uniq = 0;
          for (; it != fst_key.end (); ++it) {
            FstKey* p = *it;
            str.push_back (reinterpret_cast <char*> (p->key));
            len.push_back (p->len);
            ny::fl_t* wv = p->cont;
            if (abuse) {
              union byte_4 b4 (static_cast <float> (*wv));
              b4.u >>= 1;
              val.push_back (b4.i);
            } else {
              w.assign (&wv[0], &wv[_nl]);
              std::pair <w2id_t::iterator, bool> itb
                = w2id.insert (w2id_t::value_type (w, uniq * _nl));
              if (itb.second) {
                for (ny::uint li = 0; li < _nl; ++li)
                  _fsw[uniq * _nl + li] = wv[li]; // 0:(li=0, i=0) 1:(li=1, i=0)
                ++uniq;
              }
              val.push_back (static_cast <int> (itb.first->second)); // i*_nl
            }
            ++i;
          }
          if (j == _opt.fst_factor || _opt.fst_verbose) {
            ss.str ("");
#ifdef USE_MODEL_SUFFIX
            ss << ".-" << j;
#endif
            if (! abuse) {
              std::string fsw_fn_j  (fsw_fn_ + ss.str ());
              FILE* writer = std::fopen (fsw_fn_j.c_str (), "wb");
              std::fwrite (&_fsw[0], sizeof (ny::fl_t), uniq *_nl, writer);
              std::fclose (writer);
            }
            std::string fstrie_fn_j (fstrie_fn_ + ss.str ());
            ss.str ("");
            ss << "feature sequence trie (with 2^-" << j << " feature sequences)";
            build_trie (&_fstrie, ss.str (), fstrie_fn_j, str, len, val, _opt.verbose > 0);
            _fstrie.clear (); // if (j == _opt.fs_factor)
          }
          str.clear ();
          len.clear ();
          val.clear ();
          nkey_small >>= 1;
          if (_opt.fst_factor == j && ! _opt.fst_verbose) break;
        }
        // try reload
        if (_fstrie.open (fstrie_fn.c_str ()) != 0)
          my_errx (1, "no such double array: %s", fstrie_fn.c_str ());
        if (! abuse) {
          delete [] _fsw;
          // load computed score
          reader = std::fopen (fsw_fn.c_str (), "rb");
          if (std::fseek (reader, 0, SEEK_END) != 0) return -1;
          const size_t nfs
            = static_cast <size_t> (std::ftell (reader)) / (_nl * sizeof (ny::fl_t));
          if (std::fseek (reader, 0, SEEK_SET) != 0) return -1;
          _fsw = new ny::fl_t [_nl * nfs];
          std::fread (&_fsw[0], sizeof (ny::fl_t), _nl * nfs, reader);
          std::fclose (reader);
        }
        for (std::set <FstKey*>::const_iterator it = fst_key.begin ();
             it != fst_key.end (); ++it)
          delete *it;
        if (_opt.verbose > 0) std::fprintf (stderr, "done.\n");
#else
        my_errx(1, "%s", "On-the-fly building of fstrie is not supported in this build.");
#endif
      }
      return true;
    }
#endif
    // classification functions
    template <binary_t FLAG>
    void resetScore (double* score) const
    { _derived_const ()->template resetScore <FLAG> (score); }
    template <binary_t FLAG>
    void resetScore (double* score, const double* const score_) const
    { _derived_const ()->template resetScore <FLAG> (score, score_); }
    template <binary_t FLAG>
    void addScore (double* score, const ny::uint pos) const
    { _derived_const ()->template addScore <FLAG> (score, pos); }
    template <binary_t FLAG>
    void addScore (double* score, const int n, const ny::fl_t* const w) const
    { _derived_const ()->template addScore <FLAG> (score, n, w); }
    template <int D, bool PRUNE, binary_t FLAG>
    bool _pkePseudoInnerLoop (double* score, ny::fv_it it, const ny::fv_it& beg, const ny::fv_it& end, const ny::uint pos);
    template <int D, bool PRUNE, binary_t FLAG>
    bool _pkeInnerLoop (double* score, ny::fv_it it, const ny::fv_it& beg, const ny::fv_it& end, const size_t pos);
    template <bool PRUNE, binary_t FLAG>
    bool _pkeClassify (double* score, ny::fv_it it, const ny::fv_it& beg, const ny::fv_it& end);
    template <bool PRUNE, binary_t FLAG>
    void _pkeClassify (ny::fv_t& fv, double* score) {
      if (_d == 1) {
        _pkeClassify <false, FLAG> (score, fv.begin (), fv.begin (), fv.end ());
      } else {
        _sortFv (fv);
        if (PRUNE) _estimate_bound <FLAG> (fv.begin (), fv.begin (), fv.end ());
        _pkeClassify <PRUNE, FLAG> (score, fv.begin (), fv.begin (), fv.end ());
      }
    }
    template <bool PRUNE, binary_t FLAG>
    bool _prune (double* m, const size_t i)
    { return PRUNE && _derived ()->template prune <FLAG> (m, i); }
    template <binary_t FLAG>
    void _estimate_bound (const ny::fv_it& first, const ny::fv_it& beg, ny::fv_it it) {
      // compute bound from tail (rare) to head (common) feature
      for (size_t n = _dpolyk.size () / _d;
           n < static_cast <size_t> (std::distance (beg, it)); ++n) {
        _dpolyk.push_back (1);
        if (_d >= 2) _dpolyk.push_back (n);
        if (_d >= 3) _dpolyk.push_back (n * (n - 1) / 2);
        if (_d >= 4) _dpolyk.push_back (n * (n - 1) * (n - 2) / 6);
      }
      switch (_d) {
        case 2: _derived ()->template estimate_bound <2, FLAG> (first, beg, it); break;
        case 3: _derived ()->template estimate_bound <3, FLAG> (first, beg, it); break;
        case 4: _derived ()->template estimate_bound <4, FLAG> (first, beg, it); break;
        default: my_errx (1, HERE "_d = %d does not supported in pruning", _d);
      }
    }
    template <bool PRUNE, binary_t FLAG>
    void _fstClassify (double* score, const ny::fv_it& beg, const ny::fv_it& end);
    template <bool PRUNE, binary_t FLAG>
    void _fstClassify (ny::fv_t& fv, double* score) {
      _sortFv (fv);
      if (_d == 1)
        _fstClassify <false, FLAG> (score, fv.begin (), fv.end ());
      else
        _fstClassify <PRUNE, FLAG> (score, fv.begin (), fv.end ());
    }
    template <bool PRUNE, binary_t FLAG>
    void _pmtClassify (double* score, const ny::fv_it& beg, const ny::fv_it& end);
    template <bool PRUNE, binary_t FLAG>
    void _pmtClassify (ny::fv_t& fv, double* score) {
      _sortFv (fv);
      if (_d == 1)
        _pmtClassify <false, FLAG> (score, fv.begin (), fv.end ());
      else
        _pmtClassify <PRUNE, FLAG> (score, fv.begin (), fv.end ());
    }
    template <bool PRUNE, binary_t FLAG>
    void _baseClassify (double* score, ny::fv_it it, const ny::fv_it& beg, const ny::fv_it& end)
    { _derived ()->template baseClassify <PRUNE, FLAG> (score, it, beg, end); }
    bool _setOpt (const char* opt_str);
  public:
    ClassifierBase (const pecco::option& opt) :
      _opt (opt), _score (), _fv (), _sorter (), _d (0), _nl (0), _nf (0), _nfbit (0), _nf_cut (0), _ncf (0),  _nt (0), _tli (0), _li2l (), _l2li (), _fn2fi (), _fi2fn (), _fncnt (), _ftrie (), _fstrie (),
#ifdef USE_CEDAR
      _pmtrie (), _pmfunc (1 << _opt.pmsize, &_pmtrie), _pmi (0), _pms (0),
#ifdef USE_LFU_PMT
      _bucket_pool (), _bucket (), _item (1 << _opt.pmsize),
#else
      _ring (1 << _opt.pmsize),
#endif
#endif
      _f2dpn (0), _f2nf (0), _dpolyk (), _bound (), _fw (0), _fsw (0)
#ifdef USE_TIMER
      , _timer_pool ((std::string ("pecco profiler (") + _opt.model + ")").c_str ()), _enc_t (_timer_pool.push ("enc")), _model_t (_timer_pool.push ("model")), _classify_t (_timer_pool.push ("classify", "classify"))
#endif
#ifdef USE_PROFILING
      , _all (0), _hit (0), _lookup (0), _flen (0), _traverse (0), _lookup_sp (0), _traverse_sp (0)
#endif
    {
#ifdef USE_LFU_PMT
      _bucket_pool.create_bucket (_bucket); // root
#endif
    }
    // interface
    bool load (const char* model) { return _derived ()->load (model); }
    template <bool PRUNE, binary_t FLAG>
    void classify (ny::fv_t& fv, double* score)
    { _derived ()->template classify <PRUNE, FLAG> (fv, score); }
    bool abuse_trie () const
    { return _derived_const ()->abuse_trie (); }
    bool is_binary_classification () const
    { return _derived_const ()->is_binary_classification (); }
    void printLabel (const ny::uint li) const
    { std::fprintf (stdout, "%s", _li2l[li]); }
    void printScore (const ny::uint li, const double* score) const
    { _derived_const ()->printScore (li, score); }
    void printProb (const ny::uint li, const double* score) const
    { _derived_const ()->printProb (li, score); }
    ny::uint getLabel (const double* score)
    { return _derived ()->getLabel (score); }
    template <bool PRUNE, binary_t FLAG>
    void classify  (char* p, double* score);
    void batch     ();
    void printStat ();
  protected:
    ~ClassifierBase () {}
  };
}
#endif /* CLASSIFY_H */
