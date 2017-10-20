/**
 * author: xingshuo
 * date: 2017-10-19
 */
#include "timer.h"

#include <lua.h>
#include <lauxlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define TIMER_METATABLE "timer_metatable"

#define MAX_BUFFER_SIZE 0x100

#define GET_TIMER_MGR(L, idx) \
    (struct timer_mgr*)luaL_checkudata(L, idx, TIMER_METATABLE)

struct message {
    int session;
    int msec;
    int count;
};

struct request_package {
    uint8_t header[4]; //4 byte alignment
    struct message msg;
};

struct timer_mgr {
    int send_fd;
    int recv_fd;
    fd_set rfds;
    int loop_break;
    TimerBase* timer_base;
};

static void
block_write(int fd, char *buffer, int sz) {
    while (sz > 0) {
        int n = write(fd, buffer, sz);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            fprintf(stderr, "socket-server : write pipe error %s.\n",strerror(errno));
        }
        buffer += n;
        sz -= n;
    }
}

static void
block_read(int fd, char *buffer, int sz) {
    while (sz > 0) {
        int n = read(fd, buffer, sz);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) //if only 2 threads use one socketpair, EAGAIN is ok
                continue;
            fprintf(stderr, "socket-server : read pipe error %s.\n",strerror(errno));
            return;
        }
        buffer += n;
        sz -= n;
    }
}

static void
send_request(struct timer_mgr *m, struct request_package *request, char type, int len) {
    request->header[2] = (uint8_t)type;
    request->header[3] = (uint8_t)len;
    block_write(m->send_fd, (char*)&request->header[2], len+2);
}

static void *
base_loop(void *arg) {
    struct timer_mgr* m = arg;
    TimerBase* tb = m->timer_base;
    printf("[mstimer: %p] start loop\n", m);
    int outbuffer[MAX_BUFFER_SIZE];
    while (1) {
        if (m->loop_break) {
            break;
        }
        struct timeval* timeout = NULL;
        struct timeval cur_tv;
        struct timeval tmp;
        int n = process_timer(tb, outbuffer, MAX_BUFFER_SIZE);
        if (n > 0) {
            block_write(m->recv_fd, (char*)outbuffer, n*sizeof(int));
        }
        if (tb->size > 0) {
               GET_TIME(&cur_tv);
               if CMP_TIME(&cur_tv, &TOP_TIME(tb), >=) {
                    continue;
               }
               timeout = &tmp;
               SUB_TIME(&TOP_TIME(tb), &cur_tv, timeout);
        }
        FD_SET(m->recv_fd, &m->rfds);
        int retval = select(m->recv_fd+1, &m->rfds, NULL, NULL, timeout); //return 0 when timeout,1 when socket readable
        if (retval == 1) {
            uint8_t header[2];
            uint8_t buffer[256]; //larger than sizeof(struct request_package)
            block_read(m->recv_fd, (char*)header, sizeof(header));
            char type = header[0];
            int len = header[1];
            block_read(m->recv_fd, (char*)buffer, len);
            struct message* msg;
            switch (type) {
            case 'A':
                msg = (struct message *)buffer;
                tb->push(tb, msg->session, msg->msec, msg->count);
                break;
            case 'U':
                msg = (struct message *)buffer;
                tb->adjust(tb, msg->session, msg->msec, msg->count);
                break;
            case 'D':
                msg = (struct message *)buffer;
                tb->erase(tb, msg->session);
                break;
            case 'X':
                m->loop_break = 1;
                break;
            }
        }
    }
    release_timer(m->timer_base);
    close(m->send_fd);
    close(m->recv_fd);
    printf("[mstimer: %p] release\n", m);
    return NULL;
}


static int
_release(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    struct request_package request;
    send_request(m, &request, 'X', 0);
    return 0;
}

static int
_tostring(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    lua_pushfstring(L, "[mstimer: %p]", m);
    return 1;
}


static int
ltimer_mgr_create(lua_State *L) {
    int fd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) {
        fprintf(stderr, "wake-timer: create socket pair failed.\n");
        return 0;
    }
    struct timer_mgr* m = (struct timer_mgr*)lua_newuserdata(L, sizeof(struct timer_mgr));
    luaL_getmetatable(L, TIMER_METATABLE);
    lua_setmetatable(L, -2);
    m->send_fd = fd[0];//double channel
    m->recv_fd = fd[1];
    m->loop_break = 0;
    m->timer_base = create_timer(8);
    FD_ZERO(&m->rfds);
    assert(m->recv_fd < FD_SETSIZE);
    pthread_t pid;
    pthread_create(&pid, NULL, base_loop, m);
    return 1;
}

static int
ladd(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    int session = luaL_checkinteger(L, 2);
    int msec = luaL_checkinteger(L, 3);
    int count = luaL_checkinteger(L, 4);

    struct request_package request;
    request.msg.session = session;
    request.msg.msec = msec;
    request.msg.count = count;
    send_request(m, &request, 'A', sizeof(request.msg));
    return 0;
}

static int
lupdate(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    int session = luaL_checkinteger(L, 2);
    int msec = luaL_checkinteger(L, 3);
    int count = luaL_checkinteger(L, 4);

    struct request_package request;
    request.msg.session = session;
    request.msg.msec = msec;
    request.msg.count = count;
    send_request(m, &request, 'U', sizeof(request.msg));
    return 0;
}

static int
ldelete(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    int session = luaL_checkinteger(L, 2);

    struct request_package request;
    request.msg.session = session;
    send_request(m, &request, 'D', sizeof(request.msg));
    return 0;
}

static int
lrecv(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    int buffer;
    int r = read(m->send_fd, &buffer, sizeof(buffer));
    if (r == 0) {
        lua_pushinteger(L, -1);
        // close
        return 1;
    }
    if (r < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            return 0;
        }
        luaL_error(L, "socket error: %s", strerror(errno));
    }
    lua_pushinteger(L, buffer);
    return 1;
}

static int
lsendfd(lua_State *L) {
    struct timer_mgr* m = GET_TIMER_MGR(L, 1);
    lua_pushinteger(L, m->send_fd);
    return 1;
}

static int
lgettime(lua_State *L) {
    struct timeval v;
    GET_TIME(&v);
    double t = v.tv_sec + v.tv_usec/1.0e6;
    lua_pushnumber(L, t);
    return 1;
}


int
luaopen_mstimer(lua_State* L) {
    luaL_checkversion(L);

    luaL_Reg timer_mt[] = {
        {"__gc", _release},
        {"__tostring",  _tostring},
        {NULL, NULL}
    };

    luaL_Reg timer_methods[] = {
        {"add", ladd},
        {"update", lupdate},
        {"delete", ldelete},
        {"recv", lrecv},
        {"sendfd", lsendfd},
        {NULL, NULL},
    };

    luaL_newmetatable(L, TIMER_METATABLE);
    luaL_setfuncs(L, timer_mt, 0);
    
    luaL_newlib(L, timer_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_Reg l[] = {
        {"new", ltimer_mgr_create},
        {"gettime", lgettime},
        {NULL, NULL}
    };

    luaL_newlib(L, l);

    return 1;
}