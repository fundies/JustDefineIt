/*
typedef int x;
template<typename _Tp> struct _Array { _Array(x); };
#pragma DEBUG_ENTRY_POINT
template<typename _Tp> inline _Array<_Tp>::_Array(x __n) { }
*/

/*
template<int x> class b;
template<int x> class
#pragma DEBUG_ENTRY_POINT
b: syntax_error {};
*/

/* Simple C test case * /
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
*/
template<typename t1, typename t2> class my_template {
  t1 my_member;
  typename t2::whatever my_other_member;
};

template<typename t2> class my_template<int, t2> {
  int fantastic;
  typename t2::hahahahahaha hahahaha;
};

my_template<int, long> qool;

template<int x> class plusone {
  enum val {
    value = x + 1
  };
};

class two: plusone<1> {
  
};

/* */

/*
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <ciso646>
#include <climits>
#include <clocale>
#include <cmath>
#include <csetjmp>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <csignal>

#include <iso646.h>

#include <exception>
#include <utility>
#include <new>
#include <numeric>
*/
/*
#include <algorithm>
#include <bitset>
#include <complex>
#include <deque>
#include <fstream>
#include <functional>
#include <hash_map>
#include <hash_set>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <strstream>
#include <vector>
*/


