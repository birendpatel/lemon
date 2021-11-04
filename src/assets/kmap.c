#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wconversion"

#include <string.h>

#include "kmap.h"

/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf -t -C --null-strings --lookup-function-name=kmap_lookup keywords.txt  */
/* Computed positions: -k'1-2' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "keywords.txt"

#include "kmap.h"
#line 4 "keywords.txt"
//kv_pair defined in kmap.h

#define TOTAL_KEYWORDS 25
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 39
/* maximum key range = 37, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40,  0,  0, 20,
       0,  0,  5, 30,  0, 20, 40, 40,  0,  0,
      25,  5, 10, 40, 15,  5,  0,  5, 20, 20,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

const struct kv_pair *
kmap_lookup (register const char *str, register size_t len)
{
  static const struct kv_pair wordlist[] =
    {
      {(char*)0}, {(char*)0}, {(char*)0},
#line 18 "keywords.txt"
      {"let", _LET},
#line 11 "keywords.txt"
      {"else", _ELSE,},
#line 17 "keywords.txt"
      {"label", _LABEL},
#line 24 "keywords.txt"
      {"method", _METHOD},
#line 14 "keywords.txt"
      {"default", _DEFAULT},
#line 19 "keywords.txt"
      {"mut", _MUT},
#line 28 "keywords.txt"
      {"self", _SELF},
#line 30 "keywords.txt"
      {"false", _FALSE},
#line 20 "keywords.txt"
      {"struct", _STRUCT},
      {(char*)0},
#line 6 "keywords.txt"
      {"for", _FOR},
#line 23 "keywords.txt"
      {"func", _FUNC},
      {(char*)0},
#line 15 "keywords.txt"
      {"fallthrough", _FALLTHROUGH},
      {(char*)0},
#line 22 "keywords.txt"
      {"pub", _PUB},
#line 29 "keywords.txt"
      {"true", _TRUE},
#line 8 "keywords.txt"
      {"break", _BREAK,},
#line 25 "keywords.txt"
      {"return", _RETURN},
      {(char*)0}, {(char*)0},
#line 13 "keywords.txt"
      {"case", _CASE},
#line 7 "keywords.txt"
      {"while", _WHILE,},
#line 21 "keywords.txt"
      {"import", _IMPORT},
#line 10 "keywords.txt"
      {"if", _IF},
      {(char*)0},
#line 27 "keywords.txt"
      {"void", _VOID},
      {(char*)0},
#line 12 "keywords.txt"
      {"switch", _SWITCH},
      {(char*)0},
#line 9 "keywords.txt"
      {"continue", _CONTINUE,},
#line 26 "keywords.txt"
      {"null", _NULL},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 16 "keywords.txt"
      {"goto", _GOTO}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (s && !strncmp(str, s, len))
            return &wordlist[key];
        }
    }
  return 0;
}


#pragma GCC diagnostic pop