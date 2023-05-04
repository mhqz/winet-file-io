#include <boost/asio/windows/random_access_handle.hpp>
#include <boost/filesystem.hpp>

#include <windows.h>
#include <io.h>
#include <boost/asio/spawn.hpp>

namespace asio = boost::asio;
namespace errc = boost::system::errc;
namespace fs    = boost::filesystem;
namespace sys   = boost::system;
namespace windows = asio::windows;

using random_access_t = asio::windows::random_access_handle;
using native_handle_t = HANDLE;

namespace ouinet { namespace util { namespace file_io {

sys::error_code
last_error() {
    return make_error_code(static_cast<errc::errc_t>(errno));
}

random_access_t
open(HANDLE file, const asio::executor &exec, sys::error_code &ec) {

    random_access_t f = random_access_t(exec);
    if (file == INVALID_HANDLE_VALUE) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return f;
    }

    f.assign(file);
    //fseek(f, 0, ec);

    return f;
}

random_access_t
open_or_create(const asio::executor &exec, const fs::path &p, sys::error_code &ec) {
    HANDLE file = ::CreateFile(reinterpret_cast<LPCSTR>(p.c_str()),
                               GENERIC_READ | GENERIC_WRITE,       // DesiredAccess
                               FILE_SHARE_READ | FILE_SHARE_WRITE, // ShareMode
                               NULL,                  // SecurityAttributes
                               CREATE_ALWAYS,         // CreationDisposition
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, // FlagsAndAttributes
                               NULL);                 // TemplateFile

    return open(file, exec, ec);
}

}}}

namespace file_io = ouinet::util::file_io;

int main() {
    asio::io_context ctx;
    sys::error_code ec;

    asio::spawn(ctx, [&](asio::yield_context yield){
        auto group_name_f = file_io::open_or_create(ctx.get_executor(),
                                                    "test.txt",
                                                    ec);
    });
    ctx.run();
    return 0;
}
