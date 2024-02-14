#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include "pdep.h"

static void parse_library_options(int &i, const int argc, char **argv,
                                  std::vector<char *> &lib_args) {
  if (i < argc) {
    printf("i = %d, argc = %d\n", i, argc);
    if (std::strcmp(argv[i], "--") == 0) {  // library options
      i++;
      while (i + lib_args.size() < argc &&
             std::strcmp(argv[i + lib_args.size()], "--") != 0) {
        lib_args.push_back(argv[i + lib_args.size()]);
      }

      //printf("libsize %d\n", lib_args.size());
      i += lib_args.size();
      lib_args.push_back(nullptr);
    } else {
      fprintf(stderr, "Type `%s --help' for option details.\n", argv[0]);
      exit(-1);
    }
  }
}

int main(int argc, char **argv) {
  std::vector<char *> main_argvs;
  std::vector<char *> learner_argvs;
  std::vector<char *> chunk_argvs;
  std::vector<char *> depend_argvs;

  // main args -- [learner args] -- [chunk_args] -- [depend_args]
  //
  // find library options
  int i = 0;
  while (i < argc) {
    if (std::strcmp(argv[i], "--") == 0) {
      break;
    }

    main_argvs.push_back(argv[i]);
    i++;
  }
  // optparse use in pdep::option only use argv, so must add null terminator otherwise out-of-bounds access happens
  main_argvs.push_back(nullptr);

  if (i < argc) {
    parse_library_options(i, argc, argv, learner_argvs);
    parse_library_options(i, argc, argv, chunk_argvs);
    parse_library_options(i, argc, argv, depend_argvs);
  }

  //for (int i = 0; i < main_argvs.size(); i++) {
  //  std::cout << "main[" << i << "] = " << main_argvs[i] << "\n";
  //}

  //for (int i = 0; i < learner_argvs.size(); i++) {
  //  std::cout << "learner[" << i << "] = " << learner_argvs[i] << "\n";
  //}

  pdep::option opt(int(main_argvs.size()), main_argvs.data(),
                   int(learner_argvs.size()), learner_argvs.data(),
                   int(chunk_argvs.size()), chunk_argvs.data(),
                   int(depend_argvs.size()), depend_argvs.data());
  pdep::parser parser(opt);

  parser.run();
  return 0;
}
