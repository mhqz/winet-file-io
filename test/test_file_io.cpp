#define BOOST_TEST_MODULE Tests for file_io module
#include <boost/test/included/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include "util/signal.h"
#include "util/file_io.h"
#include "../test/util/base_fixture.hpp"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace ut = boost::unit_test;
namespace file_io = ouinet::util::file_io;

using Cancel = ouinet::Signal<void()>;

struct fixture_file_io:fixture_base
{
    asio::io_context ctx;
    sys::error_code ec;
    size_t default_timer = 2;
    Cancel cancel;

};

BOOST_FIXTURE_TEST_SUITE(suite_file_io, fixture_file_io);

BOOST_AUTO_TEST_CASE(test_open_or_create)
{
    temp_file temp_file{test_id};
        asio::spawn(ctx, [&](asio::yield_context yield){
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            auto aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    temp_file.get_name(),
                    ec);
    });
ctx.run();
    BOOST_TEST(boost::filesystem::exists(temp_file.get_name()));
}

BOOST_AUTO_TEST_CASE(test_cursor_at_end, * ut::depends_on("suite_file_io/test_open_or_create"))
{
    std::string expected_string = "0123456789";
    size_t expected_position = expected_string.size();
    temp_file temp_file{test_id};

    if (std::ofstream output{temp_file.get_name()} ) {
        output << expected_string;
    }
    BOOST_REQUIRE(boost::filesystem::exists(temp_file.get_name()));
    if (std::ifstream input{temp_file.get_name()} ) {
        std::string current_string;
        input >> current_string;
        BOOST_REQUIRE(expected_string == current_string);
    }

    asio::spawn(ctx, [&](asio::yield_context yield) {
        asio::steady_timer timer{ctx};
        timer.expires_from_now(std::chrono::seconds(default_timer));
        timer.async_wait(yield);
        async_file_handle aio_file = file_io::open_or_create(
                ctx.get_executor(),
                temp_file.get_name(),
                ec);
        size_t current_position = file_io::end_position(aio_file, ec);
        BOOST_TEST(expected_position == current_position);
    });
    ctx.run();
}

BOOST_AUTO_TEST_CASE(test_async_write)
{
    temp_file temp_file{test_id};
    std::string expected_string = "one-two-three";

    // Create the file and write at the end of the file a few times
    asio::spawn(ctx, [&](asio::yield_context yield) {
        asio::steady_timer timer{ctx};
        timer.expires_from_now(std::chrono::seconds(default_timer));
        timer.async_wait(yield);
        async_file_handle aio_file = file_io::open_or_create(
                ctx.get_executor(),
                temp_file.get_name(),
                ec);
        file_io::write(aio_file, boost::asio::const_buffer("one", 3), cancel, yield);
        file_io::write(aio_file, boost::asio::const_buffer("-two", 4), cancel, yield);
        file_io::write(aio_file, boost::asio::const_buffer("-three", 6), cancel, yield);
    });
    ctx.run();

    BOOST_REQUIRE(boost::filesystem::exists(temp_file.get_name()));
    if (std::ifstream input{temp_file.get_name()} ) {
        std::string current_string;
        input >> current_string;
        BOOST_TEST(expected_string == current_string);
    }
}

BOOST_AUTO_TEST_SUITE_END();
