#define BOOST_TEST_MODULE Tests for file_io module
#include <boost/test/included/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include "util/signal.h"
#include "util/file_io.h"

namespace asio = boost::asio;
namespace sys   = boost::system;
namespace file_io = ouinet::util::file_io;

using Cancel = ouinet::Signal<void()>;

struct ts_fixture {
    std::string testId;

    ts_fixture() {
        testId = genTestCycleID();
    }
    ~ts_fixture(){
    }

    std::string genTestCycleID(){
        std::time_t now = std::time(nullptr);
        char testCycleId[32];
        std::string dateTimeFormat = "%Y%m%d-%H%M%S-test-file-io";
        std::strftime(testCycleId,
                      32,
                      dateTimeFormat.c_str(),
                      std::gmtime(&now));
        return testCycleId;
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

struct file_io_fixture:ts_fixture
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
            file_io::write(aio_file, boost::asio::buffer(testId), cancel, yield);
        });
    }
};

BOOST_FIXTURE_TEST_SUITE(test_file_io, file_io_fixture);

BOOST_AUTO_TEST_CASE(test_open_or_create_and_write)
{
    TempFile tempFile{testId};
    spawn_open_or_create(tempFile);
    ctx.run();
    BOOST_TEST(boost::filesystem::exists(tempFile.getName()));
}

BOOST_AUTO_TEST_SUITE_END();
