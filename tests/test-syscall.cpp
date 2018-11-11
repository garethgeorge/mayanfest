#include "syscall.hpp"
#include "catch.hpp"

TEST_CASE( "Parsing path should work", "[syscall]" ) {
    MockSyscalls ms = MockSyscalls();
    ms.mknod("/foo/bar");
}