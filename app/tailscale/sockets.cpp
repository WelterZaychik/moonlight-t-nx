#include "session.hpp"
#include "tailscale.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

// --wrap applies to every static dependency, including libcurl and ENet.
// Engine sockets use Runtime::InInternal() and always reach the real stack.
extern "C" {
int __real_socket(int, int, int);
int __real_close(int);
int __real_connect(int, const sockaddr*, socklen_t);
int __real_bind(int, const sockaddr*, socklen_t);
int __real_listen(int, int);
int __real_accept(int, sockaddr*, socklen_t*);
ssize_t __real_send(int, const void*, size_t, int);
ssize_t __real_sendto(int, const void*, size_t, int, const sockaddr*, socklen_t);
ssize_t __real_recv(int, void*, size_t, int);
ssize_t __real_recvfrom(int, void*, size_t, int, sockaddr*, socklen_t*);
ssize_t __real_sendmsg(int, const msghdr*, int);
ssize_t __real_recvmsg(int, msghdr*, int);
int __real_poll(pollfd*, nfds_t, int);
int __real_shutdown(int, int);
int __real_getsockname(int, sockaddr*, socklen_t*);
int __real_getpeername(int, sockaddr*, socklen_t*);
int __real_getsockopt(int, int, int, void*, socklen_t*);
int __real_setsockopt(int, int, int, const void*, socklen_t);
int __real_fcntl(int, int, ...);
int __real_ioctl(int, unsigned long, ...);
int __real_select(int, fd_set*, fd_set*, fd_set*, timeval*);
ssize_t __real_read(int, void*, size_t);
ssize_t __real_write(int, const void*, size_t);
int __real_getaddrinfo(const char*, const char*, const addrinfo*, addrinfo**);
}

namespace moonlight::tailscale {
namespace {
class LibnxBackend final : public tsnx::Backend {
    static tsnx::Result result(int64_t value) { return {value, value < 0 ? errno : 0}; }
public:
    tsnx::Result Socket(int a, int b, int c) override { return result(__real_socket(a,b,c)); }
    tsnx::Result Close(int a) override { return result(__real_close(a)); }
    tsnx::Result Connect(int a, const sockaddr* b, socklen_t c) override { return result(__real_connect(a,b,c)); }
    tsnx::Result Bind(int a, const sockaddr* b, socklen_t c) override { return result(__real_bind(a,b,c)); }
    tsnx::Result Listen(int a, int b) override { return result(__real_listen(a,b)); }
    tsnx::Result Accept(int a, sockaddr* b, socklen_t* c) override { return result(__real_accept(a,b,c)); }
    tsnx::Result Send(int a, const void* b, size_t c, int d) override { return result(__real_send(a,b,c,d)); }
    tsnx::Result SendTo(int a, const void* b, size_t c, int d, const sockaddr* e, socklen_t f) override { return result(__real_sendto(a,b,c,d,e,f)); }
    tsnx::Result Recv(int a, void* b, size_t c, int d) override { return result(__real_recv(a,b,c,d)); }
    tsnx::Result RecvFrom(int a, void* b, size_t c, int d, sockaddr* e, socklen_t* f) override { return result(__real_recvfrom(a,b,c,d,e,f)); }
    tsnx::Result Poll(pollfd* a, nfds_t b, int c) override { return result(__real_poll(a,b,c)); }
    tsnx::Result Shutdown(int a, int b) override { return result(__real_shutdown(a,b)); }
    tsnx::Result GetSockName(int a, sockaddr* b, socklen_t* c) override { return result(__real_getsockname(a,b,c)); }
    tsnx::Result GetPeerName(int a, sockaddr* b, socklen_t* c) override { return result(__real_getpeername(a,b,c)); }
    tsnx::Result GetSockOpt(int a, int b, int c, void* d, socklen_t* e) override { return result(__real_getsockopt(a,b,c,d,e)); }
    tsnx::Result SetSockOpt(int a, int b, int c, const void* d, socklen_t e) override { return result(__real_setsockopt(a,b,c,d,e)); }
    tsnx::Result Fcntl(int a, int b, int c) override { return result(__real_fcntl(a,b,c)); }
    int DontWaitFlag() override { return MSG_DONTWAIT; }
    int NonBlockFlag() override { return O_NONBLOCK; }
    int Errno(tsnx::Err value) override {
        switch (value) {
            case tsnx::Err::Again: return EAGAIN;
            case tsnx::Err::InProgress: return EINPROGRESS;
            case tsnx::Err::ConnRefused: return ECONNREFUSED;
            case tsnx::Err::ConnReset: return ECONNRESET;
            case tsnx::Err::NotConn: return ENOTCONN;
            case tsnx::Err::BadF: return EBADF;
            case tsnx::Err::Inval: return EINVAL;
            case tsnx::Err::AddrInUse: return EADDRINUSE;
            case tsnx::Err::IsConn: return EISCONN;
            case tsnx::Err::TimedOut: return ETIMEDOUT;
            case tsnx::Err::Pipe: return EPIPE;
            case tsnx::Err::AfNoSupport: return EAFNOSUPPORT;
            case tsnx::Err::NoBufs: return ENOBUFS;
        }
        return EINVAL;
    }
};
LibnxBackend realBackend;
// A descriptor keeps its original engine alive across pause/reconfiguration.
// It must never silently migrate into another engine or onto the real stack.
std::mutex ownersMutex;
std::map<int, std::shared_ptr<Session>> owners;
}
tsnx::Backend& backend() { return realBackend; }
std::shared_ptr<Session> owner(int fd, bool erase = false) {
    if (tsnx::Runtime::InInternal()) return {};
    std::lock_guard<std::mutex> lock(ownersMutex);
    const auto found = owners.find(fd);
    if (found == owners.end()) return {};
    auto session = found->second;
    if (erase) owners.erase(found);
    return session;
}
void remember(int fd, const std::shared_ptr<Session>& session) {
    if (fd < 0) return;
    std::lock_guard<std::mutex> lock(ownersMutex);
    owners[fd] = session;
}
template<class T> T result(const tsnx::Result& value) {
    if (value.ret < 0) errno = value.err;
    return static_cast<T>(value.ret);
}
bool retired(const std::shared_ptr<Session>& session, int fd) {
    if (session && !session->alive && session->sockets->IsVirtual(fd)) { errno = ENETDOWN; return true; }
    return false;
}
}
using namespace moonlight::tailscale;

extern "C" int __wrap_socket(int domain, int type, int protocol) {
    auto session = activeSession();
    if (!session || !session->alive) return __real_socket(domain,type,protocol);
    int fd = result<int>(session->sockets->Socket(domain,type,protocol));
    remember(fd, session);
    return fd;
}
extern "C" int __wrap_close(int fd) {
    auto session = owner(fd, true);
    return session ? result<int>(session->sockets->Close(fd)) : __real_close(fd);
}
#define ROUTE(type, name, params, args, method) \
extern "C" type __wrap_##name params { \
    auto session = owner(fd); \
    if (retired(session,fd)) return -1; \
    return session ? result<type>(session->sockets->method) : __real_##name args; \
}
ROUTE(int, connect, (int fd, const sockaddr* a, socklen_t n), (fd,a,n), Connect(fd,a,n))
ROUTE(int, bind, (int fd, const sockaddr* a, socklen_t n), (fd,a,n), Bind(fd,a,n))
ROUTE(int, listen, (int fd, int n), (fd,n), Listen(fd,n))
ROUTE(ssize_t, send, (int fd, const void* b, size_t n, int f), (fd,b,n,f), Send(fd,b,n,f))
ROUTE(ssize_t, sendto, (int fd, const void* b, size_t n, int f, const sockaddr* a, socklen_t l), (fd,b,n,f,a,l), SendTo(fd,b,n,f,a,l))
ROUTE(ssize_t, recv, (int fd, void* b, size_t n, int f), (fd,b,n,f), Recv(fd,b,n,f))
ROUTE(ssize_t, recvfrom, (int fd, void* b, size_t n, int f, sockaddr* a, socklen_t* l), (fd,b,n,f,a,l), RecvFrom(fd,b,n,f,a,l))
ROUTE(int, shutdown, (int fd, int how), (fd,how), Shutdown(fd,how))
ROUTE(int, getsockname, (int fd, sockaddr* a, socklen_t* n), (fd,a,n), GetSockName(fd,a,n))
ROUTE(int, getpeername, (int fd, sockaddr* a, socklen_t* n), (fd,a,n), GetPeerName(fd,a,n))
ROUTE(int, getsockopt, (int fd, int l, int n, void* b, socklen_t* s), (fd,l,n,b,s), GetSockOpt(fd,l,n,b,s))
ROUTE(int, setsockopt, (int fd, int l, int n, const void* b, socklen_t s), (fd,l,n,b,s), SetSockOpt(fd,l,n,b,s))
#undef ROUTE
extern "C" int __wrap_accept(int fd, sockaddr* address, socklen_t* length) {
    auto session = owner(fd);
    if (retired(session,fd)) return -1;
    if (!session) return __real_accept(fd,address,length);
    const int accepted = result<int>(session->sockets->Accept(fd,address,length));
    remember(accepted,session);
    return accepted;
}
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    auto session = owner(fd);
    if (retired(session,fd)) return -1;
    return session && session->sockets->IsVirtual(fd) ? result<ssize_t>(session->sockets->Recv(fd,buffer,size,0)) : __real_read(fd,buffer,size);
}
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t size) {
    auto session = owner(fd);
    if (retired(session,fd)) return -1;
    return session && session->sockets->IsVirtual(fd) ? result<ssize_t>(session->sockets->Send(fd,buffer,size,0)) : __real_write(fd,buffer,size);
}
extern "C" int __wrap_fcntl(int fd, int command, ...) {
    // F_GETFL/F_GETFD have no third argument. Reading one is undefined.
    if (command == F_GETFL || command == F_GETFD) {
        auto session = owner(fd);
        return session && command == F_GETFL ? result<int>(session->sockets->Fcntl(fd,command,0)) : __real_fcntl(fd,command);
    }
#ifdef F_GETOWN
    if (command == F_GETOWN) return __real_fcntl(fd,command);
#endif
    va_list args; va_start(args,command);
    if (command == F_SETFL || command == F_SETFD) {
        const int argument = va_arg(args,int); va_end(args);
        auto session = owner(fd);
        return session ? result<int>(session->sockets->Fcntl(fd,command,argument)) : __real_fcntl(fd,command,argument);
    }
    auto session = owner(fd);
    if (command == F_DUPFD
#ifdef F_DUPFD_CLOEXEC
        || command == F_DUPFD_CLOEXEC
#endif
    ) {
        const int argument = va_arg(args,int); va_end(args);
        // VSock does not expose shared-open-file-description semantics.
        if (session) { errno = ENOTSUP; return -1; }
        return __real_fcntl(fd,command,argument);
    }
    if (command == F_GETLK || command == F_SETLK || command == F_SETLKW) {
        auto* argument = va_arg(args,struct flock*); va_end(args);
        if (session) { errno = ENOTSUP; return -1; }
        return __real_fcntl(fd,command,argument);
    }
    va_end(args);
    errno = ENOTSUP; return -1;
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    va_list args; va_start(args,request); void* argument = va_arg(args,void*); va_end(args);
    auto session = owner(fd);
    const int value = __real_ioctl(fd,request,argument);
    if (value == 0 && session && request == FIONBIO && argument)
        session->sockets->NoteNonBlocking(fd,*static_cast<int*>(argument) != 0);
    return value;
}
extern "C" int __wrap_poll(pollfd* descriptors, nfds_t count, int timeout) {
    if (tsnx::Runtime::InInternal()) return __real_poll(descriptors,count,timeout);
    std::shared_ptr<Session> session;
    std::vector<pollfd> copy;
    std::vector<size_t> dead;
    for (nfds_t i = 0; i < count; ++i) {
        auto candidate = owner(descriptors[i].fd);
        if (!candidate || !candidate->sockets->IsVirtual(descriptors[i].fd)) continue;
        if (!candidate->alive) dead.push_back(i);
        else session = candidate;
    }
    pollfd* working = descriptors;
    if (!dead.empty()) {
        copy.assign(descriptors,descriptors + count); working = copy.data();
        for (auto i : dead) working[i].fd = -1;
        timeout = 0;
    }
    int value = session ? result<int>(session->sockets->Poll(working,count,timeout)) : __real_poll(working,count,timeout);
    if (value < 0) return value;
    if (!dead.empty()) {
        for (nfds_t i = 0; i < count; ++i) descriptors[i].revents = working[i].revents;
        for (auto i : dead) { descriptors[i].revents = POLLERR | POLLHUP; ++value; }
    }
    return value;
}
extern "C" int __wrap_select(int limit, fd_set* read, fd_set* write, fd_set* except, timeval* timeout) {
    if (limit < 0 || limit > FD_SETSIZE || (timeout && (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000))) {
        errno = EINVAL; return -1;
    }
    bool virtualSocket = false;
    std::vector<pollfd> descriptors;
    for (int fd = 0; fd < limit; ++fd) {
        short events = 0;
        if (read && FD_ISSET(fd,read)) events |= POLLIN;
        if (write && FD_ISSET(fd,write)) events |= POLLOUT;
        if (except && FD_ISSET(fd,except)) events |= POLLPRI;
        if (!events) continue;
        descriptors.push_back({fd,events,0});
        auto session = owner(fd);
        virtualSocket |= session && session->sockets->IsVirtual(fd);
    }
    if (!virtualSocket) return __real_select(limit,read,write,except,timeout);
    int wait = timeout ? static_cast<int>(std::min<int64_t>(INT_MAX, int64_t(timeout->tv_sec) * 1000 + (timeout->tv_usec + 999) / 1000)) : -1;
    if (__wrap_poll(descriptors.data(),descriptors.size(),wait) < 0) return -1;
    for (const auto& item : descriptors) if (item.revents & POLLNVAL) { errno = EBADF; return -1; }
    if (read) FD_ZERO(read); if (write) FD_ZERO(write); if (except) FD_ZERO(except);
    int ready = 0;
    for (const auto& item : descriptors) {
        if (read && (item.events & POLLIN) && (item.revents & (POLLIN | POLLERR | POLLHUP))) { FD_SET(item.fd,read); ++ready; }
        if (write && (item.events & POLLOUT) && (item.revents & (POLLOUT | POLLERR | POLLHUP))) { FD_SET(item.fd,write); ++ready; }
        if (except && (item.events & POLLPRI) && (item.revents & POLLPRI)) { FD_SET(item.fd,except); ++ready; }
    }
    return ready;
}
namespace {
bool messageSize(const msghdr* message, size_t& size) {
    size = 0;
    if (!message || static_cast<uint64_t>(message->msg_iovlen) > 1024 || (message->msg_iovlen && !message->msg_iov)) { errno = EINVAL; return false; }
    for (size_t i = 0; i < static_cast<size_t>(message->msg_iovlen); ++i) {
        if (message->msg_iov[i].iov_len > 1024 * 1024 - size) { errno = EMSGSIZE; return false; }
        if (message->msg_iov[i].iov_len && !message->msg_iov[i].iov_base) { errno = EFAULT; return false; }
        size += message->msg_iov[i].iov_len;
    }
    return true;
}
}
extern "C" ssize_t __wrap_sendmsg(int fd, const msghdr* message, int flags) {
    auto session = owner(fd);
    if (!session || tsnx::Runtime::InInternal()) return __real_sendmsg(fd,message,flags);
    size_t size;
    if (!messageSize(message,size)) return -1;
    // sendmsg may be the first operation that virtualizes an ENet UDP socket.
    if (!session->sockets->IsVirtual(fd) && (!message->msg_name || !tsnx::VSock::IsTailnet(static_cast<const sockaddr*>(message->msg_name))))
        return __real_sendmsg(fd,message,flags);
    if (!session->alive) { errno = ENETDOWN; return -1; }
    std::vector<uint8_t> data(size);
    size_t offset = 0;
    for (size_t i = 0; i < static_cast<size_t>(message->msg_iovlen); ++i) {
        std::memcpy(data.data() + offset,message->msg_iov[i].iov_base,message->msg_iov[i].iov_len); offset += message->msg_iov[i].iov_len;
    }
    return message->msg_name ? result<ssize_t>(session->sockets->SendTo(fd,data.data(),size,flags,static_cast<const sockaddr*>(message->msg_name),message->msg_namelen))
        : result<ssize_t>(session->sockets->Send(fd,data.data(),size,flags));
}
extern "C" ssize_t __wrap_recvmsg(int fd, msghdr* message, int flags) {
    auto session = owner(fd);
    if (!session || !session->sockets->IsVirtual(fd)) return __real_recvmsg(fd,message,flags);
    if (retired(session,fd)) return -1;
    size_t size;
    if (!messageSize(message,size)) return -1;
    std::vector<uint8_t> data(size);
    socklen_t length = message->msg_namelen;
    auto value = result<ssize_t>(session->sockets->RecvFrom(fd,data.data(),size,flags,static_cast<sockaddr*>(message->msg_name),&length));
    if (value < 0) return value;
    size_t remaining = std::min<size_t>(size,value), offset = 0;
    for (size_t i = 0; i < static_cast<size_t>(message->msg_iovlen) && remaining; ++i) {
        auto count = std::min(remaining,message->msg_iov[i].iov_len);
        std::memcpy(message->msg_iov[i].iov_base,data.data() + offset,count); offset += count; remaining -= count;
    }
    // ENet always supplies an ancillary-data buffer. The virtual stack has no
    // native interface packet info to return; an empty buffer is valid.
    message->msg_controllen = 0;
    message->msg_namelen = length; message->msg_flags = static_cast<size_t>(value) > size ? MSG_TRUNC : 0;
    return value;
}
extern "C" int __wrap_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** out) {
    if (node && !tsnx::Runtime::InInternal() && (!hints || !(hints->ai_flags & AI_NUMERICHOST))) {
        auto normalize = [](std::string text) {
            if (!text.empty() && text.back() == '.') text.pop_back();
            for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            return text;
        };
        auto wanted = normalize(node);
        auto entries = peers();
        std::string matched;
        for (const auto& peer : entries) {
            auto full = normalize(peer.name), shortName = full.substr(0,full.find('.'));
            if (wanted != full && wanted != shortName) continue;
            if (!matched.empty() && matched != peer.address) return EAI_NONAME;
            matched = peer.address;
        }
        if (!matched.empty()) return __real_getaddrinfo(matched.c_str(),service,hints,out);
    }
    return __real_getaddrinfo(node,service,hints,out);
}
