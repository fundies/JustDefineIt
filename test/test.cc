
#if false

#  error Failure
int zzzzzzzzzzzzzzYOUSHOULDNOTSEETHIS;

  #if 1
    int zzzzzzzzzzzzzzzYOUSHOULDNOTSEETHIS;
  
  #else
  
    int zzzzzzzzzzzzzzzzYOUSHOULDNOTSEETHIS;
  #endif
  
#else
  int right_as_rain;

  #if 0
    int zzzzzzzzzzzzzzYOUSHOULDNOTSEETHIS;
    
  #else
    int right_as_rain1;
    
  #endif
  
  #if 0
    int zzzzzzz_YOUSHOULDNTSEETHIS;
  #elif 0
    int zzzzzzzz_YOUSHOULDNTSEETHIS;
  #elif 1
    int right_as_rain2;
  #elif 1
    int zzzzzzzzz_YOUSHOULDNTSEETHIS;
  #elif 0
    int zzzzzzzzzz_YOUSHOULDNTSEETHIS;
  #elif 1
    int zzzzzzzzzzz_YOUSHOULDNTSEETHIS;
  #else
    int zzzzzzzzzzzz_YOUSHOULDNTSEETHIS;
  #endif


const int a = 10 + 20 + 30, b, c = 5;
double d, e, f = 10;
namespace justdefineit {
  long unsigned g;
  long unsigned int h;
  long unsigned long k;
}
long double i;
class koalacub {
  int leavesEaten;
  int chutesEaten;
};
class koala: koalacub {
  koalacub *children[10]; // A koala can have at most ten children because of reasons.
};

int *level1_simple[10];
unsigned long const int   (  *  level2_better )  [ 10+5+7+9 / 3 ] ;
int* (*(*level3_confusing)[10][12])[15];
long double (*(&*level4_whatthefuck[10])[2][4])(char, short, int, long, double, long double);
long double (*(&*(***level5_okaycutitout[1^2^4^8])(int illegalnest, char (*but)(int oh, int well))[10])[2][4])(char, short, int, long, double, long double);

int my_implemented_function(int) {
  implementation
}

int my_implemented_function_2(int, double, char) {
  implementation
}
int (my_implemented_function_3)(const long double) {
  implementation
}

#endif

#if 1+1==2
  #if 2+2==3
    #error POOZER
  "#endif"
  /*#endif*/
  //#endif"
  '\
  #endif'
    #error BLEW IT
  #else
    int yes_I_can_do_math;
  #endif
#else
  #error FAILURE
#endif

/*/
#error lol

int my_overloaded_function(int) {
  implementation 1
}
int my_overloaded_function(int,int) {
  implementation 2
}

template<typename a> int function2(a b) { }

template<typename b> class templateclass { b aidu; }
/*/

int/*comment unobtrusiveness test*/comment_check;

#define ERROR_INVALID_DECLARATOR valid_declarator
int ERROR_INVALID_DECLARATOR;

#define declare_integer(x,y) int x, yes_macros_work, y
declare_integer(yes_parameters_work, yes_they_still_work);

#ifdef nonsense
  #error NOT DEFINED, REPORTED DEFINED
#else
  #define nonsense
  #ifdef nonsense
    #undef nonsense
    #ifndef nonsense
      int yes_macro_definition_works;
    #else
      #error UNDEF FAILS
    #endif
  #else
    #error NOT DEFINED, REPORTED DEFINED
  #endif
#endif

#define empty_function()
empty_function();

int zzzzz_success;
