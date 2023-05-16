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

    void spawn_open_or_create(temp_file& temp_file){
        asio::spawn(ctx, [&](asio::yield_context yield){
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            auto aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    temp_file.get_name(),
                    ec);
        });
    }

    void spawn_write(temp_file& temp_file, std::string content) {
        asio::spawn(ctx, [&](asio::yield_context yield) {
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            async_file_handle aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    temp_file.get_name(),
                    ec);
            file_io::write(aio_file, boost::asio::buffer(content), cancel, yield);
        });
    }
};

BOOST_FIXTURE_TEST_SUITE(suite_file_io, fixture_file_io);

BOOST_AUTO_TEST_CASE(test_open_or_create)
{
    temp_file temp_file{test_id};
    spawn_open_or_create(temp_file);
    ctx.run();
    BOOST_TEST(boost::filesystem::exists(temp_file.get_name()));
}

BOOST_AUTO_TEST_CASE(test_write, * ut::depends_on("suite_file_io/test_open_or_create"))
{
    std::string expected_string = test_id;
    temp_file temp_file{test_id};
    spawn_write(temp_file, expected_string);
    ctx.run();
    BOOST_REQUIRE(boost::filesystem::exists(temp_file.get_name()));
    if (std::ifstream input{temp_file.get_name()} ) {
        std::string current_string;
        input >> current_string;
        BOOST_TEST(expected_string == current_string);
    }
}

BOOST_AUTO_TEST_SUITE_END();
