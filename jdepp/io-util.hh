// SPDX-License-Identifier: MIT
// Copyright 2023-Present, Light Transport Entertainment Inc.

#include <cstdio>
#include <string>
#include <vector>
#include <cstddef> // size_t

namespace ioutil {

bool FileExists(const std::string &filepath);

bool ReadWholeFile(std::vector<uint8_t> *out, std::string *err,
                   const std::string &filepath, size_t filesize_max,
                   void *userdata);

size_t my_getline(FILE *fp, char *buf, size_t maxlen);

} // namespace ioutil
