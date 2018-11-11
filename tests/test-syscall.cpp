#include "syscall.hpp"
#include "catch.hpp"
#include <vector>

TEST_CASE( "Parsing path should work", "[syscall]" ) {
    MockSyscalls ms = MockSyscalls();
    std::vector<std::string> pp = ms.parse_path("/foo/bar/baz/bat/");
    REQUIRE(pp.size() == 4);
    REQUIRE(pp[0] == "foo");
    REQUIRE(pp[1] == "bar");
    REQUIRE(pp[2] == "baz");
    REQUIRE(pp[3] == "bat");

    pp = ms.parse_path("/foo/bar/baz/bat");
    REQUIRE(pp.size() == 4);
    REQUIRE(pp[0] == "foo");
    REQUIRE(pp[1] == "bar");
    REQUIRE(pp[2] == "baz");
    REQUIRE(pp[3] == "bat");

    pp = ms.parse_path("/foo/bar//baz/bat///");
    REQUIRE(pp.size() == 4);
    REQUIRE(pp[0] == "foo");
    REQUIRE(pp[1] == "bar");
    REQUIRE(pp[2] == "baz");
    REQUIRE(pp[3] == "bat");

    pp = ms.parse_path("//foo////bar//baz/bat");
    REQUIRE(pp.size() == 4);
    REQUIRE(pp[0] == "foo");
    REQUIRE(pp[1] == "bar");
    REQUIRE(pp[2] == "baz");
    REQUIRE(pp[3] == "bat");
}