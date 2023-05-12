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
    asio::io_context ctx;
    sys::error_code ec;
    Cancel cancel;

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

BOOST_FIXTURE_TEST_SUITE(test_file_io, ts_fixture);

BOOST_AUTO_TEST_CASE(test_open_or_create)
{
    TempFile tempFile{testId};
    asio::spawn(ctx, [&](asio::yield_context yield){
        asio::steady_timer timer{ctx};
        timer.expires_from_now(std::chrono::seconds(2));
        timer.async_wait(yield);
        file_io::open_or_create(ctx.get_executor(),
                                tempFile.getName(),
                                ec);
    });
    ctx.run();
    BOOST_TEST(boost::filesystem::exists(tempFile.getName()));
}

BOOST_AUTO_TEST_SUITE_END();
