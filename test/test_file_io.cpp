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
        test_name = ut::framework::current_test_case().p_name;
        suite_name = ut::framework::get<ut::test_suite>(ut::framework::current_test_case().p_parent_id).p_name;
        suite_id = generate_suite_id();
        test_id = suite_id + "_" + test_name;
    }
    ~fixture_base(){
    }

    std::string generate_suite_id(){
        std::time_t now = std::time(nullptr);
        char testSuiteId[32];
        std::string dateTimeFormat = "%Y%m%d-%H%M%S_" + suite_name;
        std::strftime(testSuiteId,
                      32,
                      dateTimeFormat.c_str(),
                      std::gmtime(&now));
        return testSuiteId;
    }

    struct TempFile {
        public:
            TempFile(std::string fileName){
                name = fileName;
            }
            ~TempFile(){
                if (boost::filesystem::exists(name)){
                    boost::filesystem::remove(name);
                }
            }
            std::string getName(){
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

    void spawn_open_or_create(TempFile& tempFile){
        asio::spawn(ctx, [&](asio::yield_context yield){
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            auto aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    tempFile.getName(),
                    ec);
        });
    }

    void spawn_write(TempFile& tempFile) {
        asio::spawn(ctx, [&](asio::yield_context yield) {
            asio::steady_timer timer{ctx};
            timer.expires_from_now(std::chrono::seconds(default_timer));
            timer.async_wait(yield);
            async_file_handle aio_file = file_io::open_or_create(
                    ctx.get_executor(),
                    tempFile.getName(),
                    ec);
            file_io::write(aio_file, boost::asio::buffer(suite_id), cancel, yield);
        });
    }
};

BOOST_FIXTURE_TEST_SUITE(suite_file_io, fixture_file_io);

BOOST_AUTO_TEST_CASE(test_open_or_create)
{
    TempFile tempFile{test_id};
    spawn_open_or_create(tempFile);
    ctx.run();
    BOOST_TEST(boost::filesystem::exists(tempFile.getName()));
}

//BOOST_AUTO_TEST_CASE(test_write, ut::depends_on("file_io_suite/test_open_or_create"))
BOOST_AUTO_TEST_CASE(test_write)
{
    TempFile tempFile{test_id};
    spawn_write(tempFile);
    ctx.run();
    BOOST_REQUIRE(boost::filesystem::exists(tempFile.getName()));
}

BOOST_AUTO_TEST_SUITE_END();
