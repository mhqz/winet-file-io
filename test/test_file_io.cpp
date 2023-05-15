#define BOOST_TEST_MODULE Tests for file_io module
#include <boost/test/included/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include "util/signal.h"
#include "util/file_io.h"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace ut = boost::unit_test;
namespace file_io = ouinet::util::file_io;

using Cancel = ouinet::Signal<void()>;

struct fixture_base {
    std::string test_name;
    std::string suite_name;
    std::string test_id;
    std::string suite_id;

    fixture_base() {
        auto date_time = get_date_time();
        test_name = ut::framework::current_test_case().p_name;
        suite_name = ut::framework::get<ut::test_suite>(ut::framework::current_test_case().p_parent_id).p_name;
        suite_id = date_time + "_" + suite_name;
        test_id = suite_id + "_" + test_name;
    }
    ~fixture_base(){
    }

    std::string get_date_time(){
        size_t buffer_size = 32;
        std::time_t now = std::time(nullptr);
        char testSuiteId[buffer_size];
        std::string dateTimeFormat = "%Y%m%d-%H%M%S";
        std::strftime(testSuiteId,
                      buffer_size,
                      dateTimeFormat.c_str(),
                      std::gmtime(&now));
        return testSuiteId;
    }

    struct temp_file {
        public:
            temp_file(std::string fileName){
                name = fileName;
            }
            ~temp_file(){
                if (boost::filesystem::exists(name)){
                    boost::filesystem::remove(name);
                }
            }
            std::string get_name(){
               return name;
            }
        private:
            std::string name;
    };

};

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

    void spawn_write(temp_file& temp_file) {
        asio::spawn(ctx, [&](asio::yield_context yield) {
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            async_file_handle aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    temp_file.get_name(),
                    ec);
            file_io::write(aio_file, boost::asio::buffer(suite_id), cancel, yield);
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

//BOOST_AUTO_TEST_CASE(test_write, ut::depends_on("file_io_suite/test_open_or_create"))
BOOST_AUTO_TEST_CASE(test_write)
{
    temp_file temp_file{test_id};
    spawn_write(temp_file);
    ctx.run();
    BOOST_REQUIRE(boost::filesystem::exists(temp_file.get_name()));
}

BOOST_AUTO_TEST_SUITE_END();
