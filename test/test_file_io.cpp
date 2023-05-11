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

BOOST_AUTO_TEST_CASE(test_open_or_create)
{
    asio::io_context ctx;
    sys::error_code ec;
    Cancel cancel;

    std::time_t now = std::time(nullptr);
    char testCycleId[32];
    std::strftime(testCycleId, 32, "%Y%m%d-%H%M%S-test-file-io", std::gmtime(&now));

    asio::spawn(ctx, [&](asio::yield_context yield){
        asio::steady_timer timer{ctx};
        timer.expires_from_now(std::chrono::seconds(2));
        timer.async_wait(yield);

        async_file_handle testFile = file_io::open_or_create(ctx.get_executor(),
                                                             testCycleId,
                                                             ec);
    });
    ctx.run();

    BOOST_TEST(boost::filesystem::exists(testCycleId));
}
