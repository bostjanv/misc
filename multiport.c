// http://stackoverflow.com/questions/15560336/listen-to-multiple-ports-from-one-server
// http://www.microhowto.info/howto/listen_for_and_accept_tcp_connections_in_c.html
// https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/epoll.h>

#define MAXEVENTS   64

static int verbose = 0;
static int pipe_fds[2];
static int socktype = SOCK_STREAM;

struct Port
{
    int fd;
    int in_use;
    unsigned int recv;
};

static int create_and_bind(char *port, int socktype);
static int make_socket_non_blocking(int fd);
static int epoll_loop(struct Port *ports, unsigned int first, unsigned int count);
static int accept_incoming_connections(int efd, int sfd, const char* port);
static int handle_accepted_connection(int fd);
static int get_socket_port(int fd, char sbuf[NI_MAXSERV]);
static int read_from_udp_socket(int fd, const char *port);
static void print_epoll_event(const struct epoll_event *event);
static void update_stats(struct Port *ports, int first, int count, int port, int n_bytes);
static void print_stats(struct Port *ports, int first_port, int n_ports);

static void show_usage()
{
    printf("Usage: multi [-v] [-s] [-r first_port last_port] [-p proto]\n\n"
           "  -s             increase file descriptor limit (run as root)\n"
           "  -r a b         set range of listening sockets\n"
           "  -p [tcp|udp]   set protocol\n"
           "  -v             verbose\n"
           "\n");
}

static void sighandler(int signum)
{
    char c = 'x';
    write(pipe_fds[1], &c, 1);
}

static int install_sighandler()
{
    struct sigaction sa;

    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);
    // epoll_wait can't be restarted
    // sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    return 0;
}

static int add_to_epollfd(int efd, int fd, uint32_t events)
{
    struct epoll_event event;
    
    event.data.fd = fd;
    event.events = events;
    int s = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
    if (s == -1) {
        perror ("epoll_ctl");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    unsigned int first_port = 0;
    unsigned int n_ports = (1<<15);
    int increase_fd_limit = 0;
    const char *socktype_str = "tcp";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'h') {
                show_usage();
                return 0;
            } else if (argv[i][1] == 'r') {
                if (i > argc-3) {
                    show_usage();
                    return 0;
                } else {
                    first_port = atoi(argv[++i]);
                    unsigned int last_port = atoi(argv[++i]);
                    if (last_port <= first_port) {
                        printf("Wrong range: [%d,%d)\n", first_port, last_port);
                        return 1;
                    }
                    n_ports = last_port - first_port;
                }
            } else if (argv[i][1] == 's') {
                increase_fd_limit = 1;
            } else if (argv[i][1] == 'p') {
                if (i > argc-2) {
                    show_usage();
                    return 0;
                } else {
                    i++;
                    if (!strcmp(argv[i], "tcp")) {
                        socktype = SOCK_STREAM;
                    } else if (!strcmp(argv[i], "udp")) {
                        socktype = SOCK_DGRAM;
                        socktype_str = "udp";
                    } else {
                        fprintf(stderr, "Wrong protocol: %s\n", argv[i]);
                        return 1;
                    }
                }
            } else if (argv[i][1] == 'v') {
                verbose = 1;
            }
        }
    }

    printf("Trying to open %s listening sockets on port range [%d,%d).\n",
           socktype_str, first_port, first_port+n_ports);
    printf("Keep in mind that some of the ports might be already in use by other processes ...\n");
    printf("\nPress Ctrl-C to exit ...\n\n");

    struct rlimit rl;
    struct Port *ports = NULL;

#if 0
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return 1;
    }
    printf("rlim_cur: %zu, rlim_max: %zu\n", rl.rlim_cur, rl.rlim_max);
#endif

    if (increase_fd_limit) {
        rl.rlim_max = (1<<17);
        rl.rlim_cur = (1<<17);
        if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
            perror("setrlimit");
            return 1;
        }
    }

    ports = malloc(n_ports * sizeof(struct Port));
    if (!ports) {
       fprintf(stderr, "Out of memory.\n");
       return 1;
    }

    for (int i = 0; i < n_ports; i++) {
        char port_str[6];
        snprintf(port_str, 6, "%hu", i+first_port);
        ports[i].fd = create_and_bind(port_str, socktype);
        ports[i].recv = 0;
        if (ports[i].fd == -1) {
            ports[i].in_use = 1;
            continue;
        }
        if (make_socket_non_blocking(ports[i].fd) == -1) {
            return -1;
        }
        if (socktype == SOCK_STREAM && listen(ports[i].fd, SOMAXCONN) == -1) {
            perror("listen");
            return -1;
        }
    }

#if 0
    for (;;) {
       if (network_accept_any(ports, n_ports)) {
            break;
       }
    }
#endif

    // Create pair of file descriptors needed for the self-pipe trick.
    if (pipe(pipe_fds)) {
        perror("pipe");
        goto failed;
    }

    if (install_sighandler()) {
        goto failed;
    }

    epoll_loop(ports, first_port, n_ports);

    for (size_t i = 0; i < n_ports; i++) {
        if (!ports[i].in_use) {
            if (close(ports[i].fd) == -1) {
                perror("close");
                goto failed;
            }
        }
    }

    printf("\n");
    print_stats(ports, first_port, n_ports);
    free(ports);

    close(pipe_fds[0]);
    close(pipe_fds[1]);

    return 0;

failed:
    if (ports) {
        free(ports);
    }
    return 1;
}

static int create_and_bind(char *port, int socktype)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = socktype;    /* SOCK_STREAM or SOCK_DGRAM */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0)
    {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        // Avoid "Address already in use." error.
        int yes=1;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
        } 

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            /* We managed to bind successfully! */
            s = getnameinfo(rp->ai_addr, rp->ai_addrlen,
                            hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                            NI_NUMERICHOST | NI_NUMERICSERV);
            if (s == 0 && verbose)
            {
                printf("Bind fd=%d (host=%s, port=%s)\n", sfd, hbuf, sbuf);
            }

            break;
        }

        close (sfd);
    }

    if (rp == NULL)
    {
        if (verbose) {
            fprintf (stderr, "Could not bind to port %s\n", port);
        }
        return -1;
    }

    freeaddrinfo (result);

    return sfd;
}

static int make_socket_non_blocking(int fd)
{
    int flags, s;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror ("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if (s == -1) {
        perror ("fcntl");
        return -1;
    }

    return 0;
}

static int epoll_loop(struct Port *ports, unsigned int first, unsigned int count)
{
    struct epoll_event event;
    struct epoll_event *events = NULL;

    int efd = epoll_create1(0);
    if (efd == -1) {
        perror ("epoll_create");
        return -1;
    }

    int status = -1;
    unsigned int last_listening_fd = 0;

    for (unsigned int i = 0; i < count; i++) {
        if (ports[i].in_use) {
            continue;
        }
        int fd = ports[i].fd;
        if (fd > last_listening_fd) {
            last_listening_fd = fd;
        }
        
        if (add_to_epollfd(efd, fd, EPOLLIN | EPOLLET) == -1) {
            goto end;
        }
    }

    if (add_to_epollfd(efd, pipe_fds[0], EPOLLIN | EPOLLET) == -1) {
        goto end;
    }

    /* Buffer where events are returned */
    events = calloc(MAXEVENTS, sizeof(event));

    for (;;) {
        int n = epoll_wait(efd, events, MAXEVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("epoll_wait");
            }
            goto end;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            char port[NI_NUMERICSERV] = "0";
            if (fd != pipe_fds[0]) {
                get_socket_port(fd, port);                
            }

            if (verbose) print_epoll_event(&events[i]);

            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
              /* An error has occured on this fd, or the socket is not
                 ready for reading (why were we notified then?) */
                fprintf (stderr, "epoll error on fd %d\n", events[i].data.fd);
                close(fd);
                continue;
	    } else if (fd <= last_listening_fd || fd == pipe_fds[0]) {
                if (!(events[i].events & EPOLLIN)) {
                    if (fd != pipe_fds[0]) {
                        fprintf (stderr, "epoll error on listening socket (port=%s, fd=%d)\n", port, fd);
                    } else {
                        fprintf(stderr, "epoll error on pipe file descriptor (fd=%d)\n", fd);
                        goto end;
                    }
                    continue;
                }

                if (fd == pipe_fds[0]) {
                    char c;
                    read(pipe_fds[0], &c, 1);
#if DEBUG
                    printf("Leaving epoll loop!\n");
#endif
                    status = 0;
                    goto end;
                }

                if (socktype == SOCK_STREAM) {
                    /* We have a notification on the listening socket, which
                       means one or more incoming connections. */
                    if (accept_incoming_connections(efd, fd, port) == -1) { // TODO: Get correct port.
                        goto end;
                    }
                } else {
                    //int n = read_from_udp_socket(fd, port);
                    read_from_udp_socket(fd, port);
                    update_stats(ports, first, count, atoi(port), 1);
                }
            } else {
                if (!(events[i].events & EPOLLOUT)) {
                    fprintf (stderr, "epoll error on client socket (port=%s, fd %d\n", port, fd);
                    close(fd);
                    continue;
                }
                
                if (socktype == SOCK_STREAM) {
                    /* We have data on the fd waiting to be read. Read and
                       display it. We must read whatever data is available
                       completely, as we are running in edge-triggered mode
                       and won't get a notification again for the same
                       data. */
                    handle_accepted_connection(fd);
                    update_stats(ports, first, count, atoi(port), 1);
                }
            }
        }
    }

end:
    close(efd);
    if (events) {
        free(events);
    }
    return status;
}

static int accept_incoming_connections(int efd, int sfd, const char *port)
{
    int fd = -1;

    for (;;) {
        struct sockaddr in_addr;
        socklen_t in_len;
        int fd;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        struct epoll_event event;

        in_len = sizeof in_addr;
        fd = accept(sfd, &in_addr, &in_len);
        if (fd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* We have processed all incoming
                   connections. */
                break;
            } else {
                perror ("accept");
                goto failed;
            }
        }

        int s = getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
        if (s == 0 && verbose) {
            printf("Accepted connection on port %s (fd=%d, host=%s, port=%s)\n", port, fd, hbuf, sbuf);
        }

        /* Make the incoming socket non-blocking and add it to the
           list of fds to monitor. */
        s = make_socket_non_blocking(fd);
        if (s == -1) {
            goto failed;
        }

        event.data.fd = fd;
        event.events = EPOLLOUT | EPOLLET;
        s = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
        if (s == -1) {
            perror ("epoll_ctl");
            goto failed;
        }
    }

    return 0;

failed:
    if (fd != -1) {
        close(fd);
    }
    return -1;
}

static int handle_accepted_connection(int fd)
{
    const char msg[] = "MULTI\r\n";

    if (send(fd, msg, sizeof(msg)-1, MSG_NOSIGNAL) != sizeof(msg)-1) {
        perror("send");
    }

    if (close(fd) == -1) {
        perror("close");
        return -1;
    }

    printf(".");
    fflush(stdout);

    return 0;
}

static int read_from_udp_socket(int fd, const char *port)
{
    char buf[1024];
    struct sockaddr in_addr;
    socklen_t in_len;
    char sbuf[NI_MAXSERV];
    char hbuf[NI_MAXHOST];

    ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, &in_addr, &in_len);
    int s = getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (s == 0) {
        if (verbose) {
            printf("Read %ld bytes on port %s (fd=%d, host=%s, port=%s)\n", n, port, fd, hbuf, sbuf);
        }
    } else {
        perror("getnameinfo");
    }

    if (n > 0) {
        if (verbose) {
            printf("%.*s\n", (int)n, buf);
        } else {
            printf(".");
            fflush(stdout);
        }
    }

    return n;
}

static int get_socket_port(int fd, char sbuf[NI_MAXSERV])
{
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(struct sockaddr);
    char hbuf[NI_MAXHOST];

    int s = getsockname(fd, &in_addr, &in_len);
    if (s == -1) {
        perror("getsockname");
        return -1;
    }

    s = getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, NI_MAXHOST, NI_NUMERICHOST | NI_NUMERICSERV);
    if (s != 0) {
        perror("getnameinfo");
        return -1;
    }

    return 0;
}

static const struct event_table
{
    int event;
    char *name;
} event_table[] =
{
    {EPOLLIN,      "EPOLLIN" },
    {EPOLLOUT,     "EPOLLOUT"},
    {EPOLLRDHUP,   "EPOLLRDHUP"},
    {EPOLLPRI,     "EPOLLPRI"},
    {EPOLLERR,     "EPOLLERR"},
    {EPOLLHUP,     "EPOLLHUP"},
    {EPOLLET,      "EPOLLET"},
    {EPOLLONESHOT, "EPOLLONESHOT"},
    {EPOLLWAKEUP,  "EPOLLWAKEUP"},
};

static void print_epoll_event(const struct epoll_event *event)
{
    printf("event: fd=%d", event->data.fd);
    
    for (int i = 0; i < sizeof(event_table) / sizeof(*event_table); i++) {
        if (event->events & event_table[i].event) {
            printf(" %s", event_table[i].name);
        }
    }
    printf("\n");
}

static void update_stats(struct Port *ports, int first_port, int n_ports, int port, int n_bytes)
{
    assert(port - first_port < n_ports);
    ports[port - first_port].recv += n_bytes;
}

static void print_stats(struct Port *ports, int first_port, int n_ports)
{
    for (int i = 0; i < n_ports; i++) {
        if (ports[i].recv > 0) {
            printf("%-4d: %u\n", i + first_port, ports[i].recv);
        }
    }
}

#if 0
/*
static int create_and_bind_simple(unsigned short port)
{
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in sa4;
    memset(&sa4, 0, sizeof(sa4));
    sa4.sin_port = htons(port);
    sa4.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuseaddr=1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
        perror("setsockopt");
        goto failed;
    }

    if (bind(fd, (const struct sockaddr*)&sa4, sizeof(sa4)) == -1) {
        if (errno == EADDRINUSE) {
            if (verbose) {
                printf("Port %hu already in use.\n", port);
            }
        } else {
            perror("bind");
        }
        goto failed;
    } else {
        if (listen(fd, BACKLOG) == -1) {
            perror("listen");
            goto failed;
        }
    }

    return fd;

failed:
    close(fd);
    return -1;
}

static int network_accept_any(struct Port *ports, unsigned int count)
{
    fd_set readfds;
    int maxfd, ret;
    struct sockaddr_in sa4;
    socklen_t len = sizeof(sa4);

    // TODO: Replace select with epoll since select can't handle more that FD_SETSIZE sockets.

    FD_ZERO(&readfds);
    maxfd = -1;
    for (unsigned i = 0; i < count; i++) {
        if (!ports[i].in_use) {
            if (ports[i].fd >= FD_SETSIZE) {
                fprintf(stderr, "Can't have more than %d sockets in a fd_set.\n", FD_SETSIZE);
                return 1;
            }
            FD_SET(ports[i].fd, &readfds);
            if (ports[i].fd > maxfd) {
                maxfd = ports[i].fd;
            }
        }
    }
    if ((ret = select(maxfd + 1, &readfds, NULL, NULL, NULL)) == -1) {
        perror("select");
        return 1;
    }

    const unsigned int deadbeaf = htonl(0xdeadbeaf);

    for (unsigned int i = 0; i < count; i++) {
        if (FD_ISSET(ports[i].fd, &readfds)) {
            int fd = accept(ports[i].fd, (struct sockaddr*)&sa4, &len);
            if (fd == -1) {
                perror("accept");
                return 1;
            }
            printf("Sending to %u.\n", i+FIRST_PORT);
            if (send(fd, &deadbeaf, sizeof(deadbeaf), MSG_NOSIGNAL) != sizeof(deadbeaf)) {
                perror("send");
                return 1;
            }
            if (close(fd) == -1) {
                perror("close");
                return 1;
            }
        }
    }

    return 0;
}
*/
#endif
