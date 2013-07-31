#include <stdio.h>
#include <stdlib.h>

#undef NDEBUG
#include <assert.h>

struct {
  size_t num_performed;
  size_t num_passed;
} tests_state;

#define testing() \
  void _runtests(); \
  int main() { \
    tests_state.num_performed = 0; \
    tests_state.num_passed = 0; \
    _runtests(); \
    return tests_state.num_passed == tests_state.num_performed ? 0 : -1; \
  } \
  void _runtests()

void test_pass( const char* name ) {
  ++tests_state.num_performed;
  ++tests_state.num_passed;
  
  //printf( "ok %lu: %s\n", tests_state.num_performed, name );
}

void test_fail( const char* name, const char* filename, size_t line ) {
  ++tests_state.num_performed;
  
  printf( "FAIL: %s. File %s, line %lu\n", name, filename, line );
}

int test_ok( int res, const char* name, const char* filename, size_t line ) {
  if( res ) {
    test_pass( name );
    return 1;
  }
  else {
    test_fail( name, filename, line );
    return 0;
  }
}

#define ok( res ) test_ok( res, #res, __FILE__, __LINE__ )
#define pass( name ) test_pass( name )
#define fail( name ) test_fail( name, __FILE__, __LINE__ )
