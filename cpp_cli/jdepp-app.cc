#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include "pdep.h"

int main (int argc, char** argv) {

  pdep::option opt (argc, argv);
  pdep::parser parser (opt);

  parser.run ();
  return 0;
}
