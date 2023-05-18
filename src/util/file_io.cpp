#include <boost/asio/read.hpp>
#include <boost/asio/read_at.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/write_at.hpp>

#include "file_io.h"

namespace ouinet { namespace util { namespace file_io {

namespace errc = boost::system::errc;

static
sys::error_code last_error()
{
    return make_error_code(static_cast<errc::errc_t>(errno));
}

void
fseek(async_file_handle& file_handle, size_t pos, sys::error_code& ec)
{
    if(!fseek_native(file_handle, pos))
    {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
}

#ifdef _WIN32
bool
fseek_native(async_file_handle& file_handle, size_t pos) {
    native_handle_t native_handle = file_handle.native_handle();
    bool success;
    success = INVALID_SET_FILE_POINTER !=  SetFilePointer(native_handle, pos, NULL, FILE_BEGIN);
    return success;
}

size_t
current_position(async_file_handle& f, sys::error_code& ec)
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
end_position(async_file_handle& f, sys::error_code& ec)
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

size_t
file_size(async_file_handle& f, sys::error_code& ec)
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
file_remaining_size(async_file_handle& f, sys::error_code& ec)
{
    auto size = file_size(f, ec);
    if (ec) return 0;

    auto pos = current_position(f, ec);
    if (ec) return 0;

    return size - pos;
}

async_file_handle
open(native_handle_t file, const asio::executor &exec, sys::error_code &ec) {

    async_file_handle f = async_file_handle(exec);
    if (file == INVALID_HANDLE_VALUE) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return f;
    }

    f.assign(file);
    fseek(f, 0, ec);

    return f;
}

async_file_handle
open_or_create(const asio::executor &exec, const fs::path &p, sys::error_code &ec) {
    native_handle_t file = ::CreateFile(p.string().c_str(),
                               GENERIC_READ | GENERIC_WRITE,       // DesiredAccess
                               FILE_SHARE_READ | FILE_SHARE_WRITE, // ShareMode
                               NULL,                  // SecurityAttributes
                               OPEN_ALWAYS,         // CreationDisposition
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, // FlagsAndAttributes
                               NULL);                 // TemplateFile

    return open(file, exec, ec);
}

async_file_handle
open_readonly( const asio::executor& exec
             , const fs::path& p
             , sys::error_code& ec)
{
    native_handle_t file = ::CreateFile(p.string().c_str(),
                               GENERIC_READ,        // DesiredAccess
                               FILE_SHARE_READ,     // ShareMode
                               NULL,                // SecurityAttributes
                               OPEN_ALWAYS,         // CreationDisposition
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_READONLY, // FlagsAndAttributes
                               NULL);               // TemplateFile
    return open(file, exec, ec);
}

native_handle_t dup_fd(async_file_handle& f, sys::error_code& ec)
{
    native_handle_t file;
    if(!::DuplicateHandle(
            ::GetCurrentProcess(), // hSourceProcessHandle
            f.native_handle(), // hSourceHandle
            ::GetCurrentProcess(), // hTargetProcessHandle
            &file, // lpTargetHandle
            0, // dwDesiredAccess
            FALSE, // bInheritHandle
            DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS //dwOptions
    )) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
    return file;
}

void
truncate(async_file_handle& f
        , size_t new_length
        , sys::error_code& ec)
{
    fseek(f, new_length, ec);
    if (!::SetEndOfFile(f.native_handle())) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
}

void
read(async_file_handle& f
    , asio::mutable_buffer b
    , Cancel& cancel
    , asio::yield_context yield)
{
    auto cancel_slot = cancel.connect([&] { f.close(); });
    sys::error_code ec;
    asio::async_read_at(f, 0, b, yield[ec]);
    return_or_throw_on_error(yield, cancel, ec);
}

void
write_at(async_file_handle& f
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
write_at_end(async_file_handle& f
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
write(async_file_handle& f
        , asio::const_buffer b
        , Cancel& cancel
        , asio::yield_context yield)
{
    write_at_end(f, b, cancel, yield);
}

#endif
}}} // namespaces
