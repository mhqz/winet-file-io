#include <boost/asio/windows/random_access_handle.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>


#include "util/signal.h"

#include <windows.h>
#include <io.h>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write_at.hpp>
#include <boost/asio/steady_timer.hpp>

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

void
fseek(random_access_t &file_handle, size_t pos, sys::error_code& ec)
{
    native_handle_t native_handle = file_handle.native_handle();
    if(INVALID_SET_FILE_POINTER ==  SetFilePointer(native_handle, pos, NULL, FILE_BEGIN))
    {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
}

size_t
current_position(random_access_t& f, sys::error_code& ec)
{
    native_handle_t native_handle = f.native_handle();
    auto offset = SetFilePointer(native_handle, 0, NULL, FILE_CURRENT);
    if(INVALID_SET_FILE_POINTER ==  offset)
    {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return size_t(-1);
    }

    return offset;
}

size_t
end_position(random_access_t& f, sys::error_code& ec)
{
    native_handle_t native_handle = f.native_handle();
    auto offset = SetFilePointer(native_handle, 0, NULL, FILE_END);
    if(INVALID_SET_FILE_POINTER ==  offset)
    {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return size_t(-1);
    }

    return offset;
}


//XXX: Improvements are required to avoid code repetition and unify fseek
// behavior with posix without sacrificing performance in the windows implementation
size_t
file_size(random_access_t& f, sys::error_code& ec)
{
    auto start_pos = current_position(f, ec);
    if (ec) return size_t(-1);

    auto end = end_position(f, ec);
    if (ec) return size_t(-1);

    fseek(f, start_pos, ec);
    if (ec) return size_t(-1);

    return end;
}

size_t
file_remaining_size(random_access_t & f, sys::error_code& ec)
{
    auto size = file_size(f, ec);
    if (ec) return 0;

    auto pos = current_position(f, ec);
    if (ec) return 0;

    return size - pos;
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
    fseek(f, 0, ec);

    return f;
}

random_access_t
open_or_create(const asio::executor &exec, const fs::path &p, sys::error_code &ec) {
    HANDLE file = ::CreateFile(p.string().c_str(),
                               GENERIC_READ | GENERIC_WRITE,       // DesiredAccess
                               FILE_SHARE_READ | FILE_SHARE_WRITE, // ShareMode
                               NULL,                  // SecurityAttributes
                               OPEN_ALWAYS,         // CreationDisposition
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, // FlagsAndAttributes
                               NULL);                 // TemplateFile

    return open(file, exec, ec);
}

//XXX: Avoid code repetition on open_or_create
//TODO: Add unit tests
random_access_t
open_readonly( const asio::executor& exec
        , const fs::path& p
        , sys::error_code& ec)
{
    HANDLE file = ::CreateFile(p.string().c_str(),
                               GENERIC_READ,        // DesiredAccess
                               FILE_SHARE_READ,     // ShareMode
                               NULL,                // SecurityAttributes
                               OPEN_ALWAYS,         // CreationDisposition
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_READONLY, // FlagsAndAttributes
                               NULL);               // TemplateFile
    return open(file, exec, ec);
}

void
write_at(random_access_t& f
        , asio::const_buffer b
        , uint64_t offset
        , Cancel& cancel
        , asio::yield_context yield)
{
    auto cancel_slot = cancel.connect([&] { f.close(); });
    sys::error_code ec_write;
    asio::async_write_at(f, offset, b, [&ec_write](const boost::system::error_code& ec,
                                     std::size_t bytes_transferred){ec_write = std::move(ec);});
    return_or_throw_on_error(yield, cancel, ec_write);
}

void
write_at_end(random_access_t& f
        , asio::const_buffer b
        , Cancel& cancel
        , asio::yield_context yield)
{
    sys::error_code ec;
    uint64_t offset = end_position(f, ec);
    write_at(f, b, offset, cancel, yield);
    return_or_throw_on_error(yield, cancel, ec);
}

void
write(random_access_t& f
            , asio::const_buffer b
            , Cancel& cancel
            , asio::yield_context yield)
{
    write_at_end(f, b, cancel, yield);
}


}}}

namespace file_io = ouinet::util::file_io;
using Cancel = ouinet::Signal<void()>;

int main() {
    asio::io_context ctx;
    sys::error_code ec;
    Cancel cancel;

    asio::spawn(ctx, [&](asio::yield_context yield){
        asio::steady_timer timer{ctx};
        timer.expires_from_now(std::chrono::seconds(2));
        timer.async_wait(yield);

        auto group_name_f = file_io::open_or_create(ctx.get_executor(),
                                                    "prueba.txt",
                                                    ec);

        file_io::write(group_name_f, boost::asio::buffer(" "), cancel, yield);
        file_io::write(group_name_f, boost::asio::buffer("hola"), cancel, yield);
        file_io::write(group_name_f, boost::asio::buffer(" "), cancel, yield);
        file_io::write(group_name_f, boost::asio::buffer("mundo"), cancel, yield);
    });
    ctx.run();
    return 0;
}
