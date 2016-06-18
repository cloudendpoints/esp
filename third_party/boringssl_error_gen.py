# Copyright (c) 2015, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""
Reimplementation of error table generator Boring SSL uses in build.

Boring SSL implementation is in go which doesn't yet have complete Bazel
support and the temporary support used in nginx workspace
https://nginx.googlesource.com/workspace does not work well with
Bazel sandboxing. Therefore, we temporarily reimplement the error
table generator.
"""

import sys

# _LIBRARY_NANES must be kept in sync with the enum in err.h.
# The generated code will contain static assertions to enforce this.
_LIBRARY_NAMES = [
  "NONE",
  "SYS",
  "BN",
  "RSA",
  "DH",
  "EVP",
  "BUF",
  "OBJ",
  "PEM",
  "DSA",
  "X509",
  "ASN1",
  "CONF",
  "CRYPTO",
  "EC",
  "SSL",
  "BIO",
  "PKCS7",
  "PKCS8",
  "X509V3",
  "RAND",
  "ENGINE",
  "OCSP",
  "UI",
  "COMP",
  "ECDSA",
  "ECDH",
  "HMAC",
  "DIGEST",
  "CIPHER",
  "HKDF",
  "USER",
]

class BoringSslException(Exception):
  pass

# _OFFSET_MASK is the bottom 15 bits. It's a mask that selects the offset
# from an int stored in _entries.
_OFFSET_MASK = 0x7fff

class StringList(object):
  """StringList is a map from int -> string which can output data for a
  sorted list as C literals.
  """

  def __init__(self):
    # _entries is an array of keys and offsets into `_string_data`. The
    # offsets are in the bottom 15 bits of each int and the key is the
    # top 17 bits.
    self._entries = []   # of int

    # _interned_strings contains the same strings as are in `_string_data`,
    # but allows for easy deduplication. It maps a string to its offset in
    # `_string_data`.
    self._interned_strings = {}  # str -> int
    self._string_data = []       # of character

  def add(self, key, value):
    if key & _OFFSET_MASK != 0:
      raise BoringSslException("need bottom 15 bits of the key for the offset")

    offset = self._interned_strings.get(value)
    if offset is None:
      offset = len(self._string_data)
      if offset & _OFFSET_MASK != offset:
        raise BoringSslException("StringList overflow")

      self._string_data.extend(list(value))
      self._string_data.append(0)

      self._interned_strings[value] = offset

      for existing in self._entries:
        if existing >> 15 == key >> 15:
          raise BoringSslException("duplicate entry")

    self._entries.append(key | offset)

  def build_list(self):
    return sorted(self._entries, lambda x,y: cmp(x >> 15, y >> 15))

  def write_to(self, out, name):
    list = self.build_list()
    sys.stderr.write(
        "{}: {} bytes of list and {} bytes of string data.\n".format(
            name, 4 * len(list), len(self._string_data)))

    values = "kOpenSSL" + name + "Values"
    out.write("const uint32_t {}[] = {{\n".format(values))
    for v in list:
      out.write("    0x{:x},\n".format(v))
    out.write("};\n\n")

    out.write("const size_t {0}Len = sizeof({0}) / sizeof({0}[0]);\n\n".format(
        values))

    string_data = "kOpenSSL" + name + "StringData"
    out.write("const char {}[] =\n    \"".format(string_data))
    for c in self._string_data:
      if c == 0:
        out.write("\\0\"\n    \"")
      else:
        out.write(c)

    out.write("\";\n\n")

class ErrorData(object):
  def __init__(self, lib_names):
    self._reasons = StringList()
    self._library_map = {}  # str -> int

    i = 1
    for name in lib_names:
      self._library_map[name] = i
      i += 1

  def read_errordata_file(self, filename):
    with open(filename, "r") as errordata:
      line_number = 0
      for line in errordata:
        line_number += 1
        line = line.strip()

        if not line: continue
        parts = line.split(',')

        if len(parts) != 3:
          raise BoringSslException(
              "bad line {} in {}: found {} values but want 3".format(
                  line_number, filename, len(parts)))

        name, key, value = parts

        library_number = self._library_map.get(name)
        if library_number is None:
          raise BoringSslException(
              "bad line {} in {}: unknown library".format(
                  line_number, filename))

        if library_number >= 64:
          raise BoringSslException(
              "bad line {} in {}: library value too large".format(
                  line_number, filename))

        try:
          key = int(key)
        except ValueError, e:
          raise BoringSslException("bad line {} in {}: {}".format(
              line_number, filename, e))

        if key >= 2048:
          raise BoringSslException("bad line {} in {}: key too large".format(
              line_number, filename))

        list_key = library_number << 26 | key << 15
        self._reasons.add(list_key, value)


  def write_to(self, out, name):
    self._reasons.write_to(out, name)


def main():
  inputs = sorted(filter(lambda n: n and n.endswith(".errordata"), sys.argv))

  e = ErrorData(_LIBRARY_NAMES)

  out = sys.stdout
  out.write("""/* Copyright (c) 2015, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

 /* This file was generated by err_data_generate.go. */

#include <openssl/base.h>
#include <openssl/err.h>
#include <openssl/type_check.h>


""")

  for i in inputs:
    e.read_errordata_file(i)

  i = 1
  for name in _LIBRARY_NAMES:
    out.write("OPENSSL_COMPILE_ASSERT(ERR_LIB_{0} == {1}, "
              "library_values_changed_{1});\n".format(name, i))
    i += 1

  out.write("OPENSSL_COMPILE_ASSERT(ERR_NUM_LIBS == {}, "
            "library_values_changed_num);\n".format(i))
  out.write("\n")

  e.write_to(out, "Reason")


if __name__ == "__main__":
  main()

