#include "catch.hpp"

#include <osmium/index/detail/typed_mmap.hpp>

#if defined(_MSC_VER) || (defined(__GNUC__) && defined(_WIN32))
  #include "win_mkstemp.hpp"
#endif

TEST_CASE("TypedMmap") {

SECTION("Mmap") {
    uint64_t* data = osmium::detail::typed_mmap<uint64_t>::map(10);

    data[0] = 4ul;
    data[3] = 9ul;
    data[9] = 25ul;

    REQUIRE(4ul == data[0]);
    REQUIRE(9ul == data[3]);
    REQUIRE(25ul == data[9]);

    osmium::detail::typed_mmap<uint64_t>::unmap(data, 10);
}

SECTION("MmapSizeZero") {
    REQUIRE_THROWS_AS(osmium::detail::typed_mmap<uint64_t>::map(0), std::system_error);
}

SECTION("MmapHugeSize") {
    REQUIRE_THROWS_AS(osmium::detail::typed_mmap<uint64_t>::map(1L << 50), std::system_error);
}

#ifdef __linux__
SECTION("Remap") {
    uint64_t* data = osmium::detail::typed_mmap<uint64_t>::map(10);

    data[0] = 4ul;
    data[3] = 9ul;
    data[9] = 25ul;

    uint64_t* new_data = osmium::detail::typed_mmap<uint64_t>::remap(data, 10, 1000);

    REQUIRE(4ul == new_data[0]);
    REQUIRE(9ul == new_data[3]);
    REQUIRE(25ul == new_data[9]);
}
#else
# pragma message "not running 'Remap' test case on this machine"
#endif

SECTION("FileSize") {
    const int size = 100;
    char filename[] = "/tmp/osmium_unit_test_XXXXXX";
    const int fd = mkstemp(filename);
    REQUIRE(fd > 0);
    REQUIRE(0 == unlink(filename));
    REQUIRE(0 == osmium::detail::typed_mmap<uint64_t>::file_size(fd));
    REQUIRE(0 == ftruncate(fd, size * sizeof(uint64_t)));
    REQUIRE(size == osmium::detail::typed_mmap<uint64_t>::file_size(fd));

    osmium::detail::typed_mmap<uint64_t>::grow_file(size / 2, fd);
    REQUIRE(size == osmium::detail::typed_mmap<uint64_t>::file_size(fd));

    osmium::detail::typed_mmap<uint64_t>::grow_file(size, fd);
    REQUIRE(size == osmium::detail::typed_mmap<uint64_t>::file_size(fd));

    osmium::detail::typed_mmap<uint64_t>::grow_file(size * 2, fd);
    REQUIRE((size * 2) == osmium::detail::typed_mmap<uint64_t>::file_size(fd));
}

SECTION("GrowAndMap") {
    const int size = 100;
    char filename[] = "/tmp/osmium_unit_test_XXXXXX";
    const int fd = mkstemp(filename);
    REQUIRE(fd > 0);
    REQUIRE(0 == unlink(filename));

    uint64_t* data = osmium::detail::typed_mmap<uint64_t>::grow_and_map(size, fd);
    REQUIRE(size == osmium::detail::typed_mmap<uint64_t>::file_size(fd));

    data[0] = 1ul;
    data[1] = 8ul;
    data[99] = 27ul;

    REQUIRE(1ul == data[0]);
    REQUIRE(8ul == data[1]);
    REQUIRE(27ul == data[99]);

    osmium::detail::typed_mmap<uint64_t>::unmap(data, size);
}

}
