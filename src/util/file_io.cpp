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
open(HANDLE file, const asio::executor &exec, sys::error_code &ec) {

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
    HANDLE file = ::CreateFile(p.string().c_str(),
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

/***
size_t current_position(async_file_handle& f, sys::error_code& ec)
{
    off_t offset = lseek(f.native_handle(), 0, SEEK_CUR);

    if (offset == -1) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return size_t(-1);
    }

    return offset;
}

size_t file_size(async_file_handle& f, sys::error_code& ec)
{
    auto start_pos = current_position(f, ec);
    if (ec) return size_t(-1);

    if (lseek(f.native_handle(), 0, SEEK_END) == -1) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }

    auto end = current_position(f, ec);
    if (ec) return size_t(-1);

    fseek(f, start_pos, ec);
    if (ec) return size_t(-1);

    return end;
}

size_t file_remaining_size(async_file_handle& f, sys::error_code& ec)
{
    auto size = file_size(f, ec);
    if (ec) return 0;

    auto pos = current_position(f, ec);
    if (ec) return 0;

    return size - pos;
}

static
async_file_handle open( int file
                             , const asio::executor& exec
                             , sys::error_code& ec)
{
    if (file == -1) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
        return asio::async_file_handle(exec);
    }

    asio::async_file_handle f(exec, file);
    fseek(f, 0, ec);

    return f;
}

async_file_handle open_or_create( const asio::executor& exec
                                       , const fs::path& p
                                       , sys::error_code& ec)
{
    int file = ::open(p.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    return open(file, exec, ec);
}

async_file_handle open_readonly( const asio::executor& exec
                                      , const fs::path& p
                                      , sys::error_code& ec)
{
    int file = ::open(p.c_str(), O_RDONLY);
    return open(file, exec, ec);
}

int dup_fd(asio::async_file_handle& f, sys::error_code& ec)
{
    int file = ::dup(f.native_handle());
    if (file == -1) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
    return file;
}

void truncate( async_file_handle& f
             , size_t new_length
             , sys::error_code& ec)
{
    if (ftruncate(f.native_handle(), new_length) != 0) {
        ec = last_error();
        if (!ec) ec = make_error_code(errc::no_message);
    }
}

bool check_or_create_directory(const fs::path& dir, sys::error_code& ec)
{
    // https://www.boost.org/doc/libs/1_69_0/libs/system/doc/html/system.html#ref_boostsystemerror_code_hpp

    namespace errc = boost::system::errc;

    if (fs::exists(dir)) {
        if (!is_directory(dir)) {
            ec = make_error_code(errc::not_a_directory);
            return false;
        }

        return false;
    }
    else {
        if (!create_directories(dir, ec)) {
            if (!ec) ec = make_error_code(errc::operation_not_permitted);
            return false;
        }
        assert(is_directory(dir));
        return true;
    }
}

void read( async_file_handle& f
         , asio::mutable_buffer b
         , Cancel& cancel
         , asio::yield_context yield)
{
    auto cancel_slot = cancel.connect([&] { f.close(); });
    sys::error_code ec;
    asio::async_read(f, b, yield[ec]);
    return_or_throw_on_error(yield, cancel, ec);
}

void write( async_file_handle& f
          , asio::const_buffer b
          , Cancel& cancel
          , asio::yield_context yield)
{
    auto cancel_slot = cancel.connect([&] { f.close(); });
    sys::error_code ec;
    asio::async_write(f, b, yield[ec]);
    return_or_throw_on_error(yield, cancel, ec);
}

void remove_file(const fs::path& p)
{
    if (!exists(p)) return;
    assert(is_regular_file(p));
    if (!is_regular_file(p)) return;
    sys::error_code ignored_ec;
    fs::remove(p, ignored_ec);
}

***/

}}} // namespaces
