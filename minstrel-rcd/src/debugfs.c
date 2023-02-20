#include <sys/socket.h>
#include <netinet/in.h>
#include <libubox/usock.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "rcd.h"

static LIST_HEAD(mon_list);

struct mon_context {
	struct list_head list;
	bool compression;
	int port;
	struct uloop_fd mon_fd;
	struct uloop_fd sfd;
	struct zstd_buf buf;
	struct list_head clients;
};

struct mon_client {
	struct list_head list;
	struct ustream_fd sfd;
	struct mon_context *ctx;
};

// static void
// mon_client_notify_read(struct ustream *s, int bytes)
// {
// 	struct mon_client *cl = container_of(s, struct mon_client, sfd.stream);
// 	char *data
// 	int len;

// 	while (1) {
// 		data = ustream_get_read_buf(s, &len);
// 		if (!data)
// 			return;

// 		len = mon_client_handle
// 	}
// }

static void
mon_stop(struct mon_context *ctx)
{
	struct mon_client *cur, *next;

	uloop_timeout_cancel(&ctx->buf.timeout);

	list_for_each_entry_safe(cur, next, &ctx->clients, list) {
		ustream_free(&cur->sfd.stream);
		close(cur->sfd.fd.fd);
		list_del(&cur->list);
		free(cur);
	}

	free(ctx->buf.in.buf);
	uloop_fd_delete(&ctx->sfd);
	close(ctx->sfd.fd);
	uloop_fd_delete(&ctx->mon_fd);
	close(ctx->mon_fd.fd);
	list_del(&ctx->list);
	free(ctx);
}

void
rcd_debugfs_monitoring_stop(void)
{
	struct mon_context *cur, *next;

	list_for_each_entry_safe(cur, next, &mon_list, list)
		mon_stop(cur);
}

static void
mon_client_notify_state(struct ustream *s)
{
	struct mon_client *cl = container_of(s, struct mon_client, sfd.stream);
	struct mon_context *ctx = cl->ctx;

	if (!s->write_error && !s->eof)
		return;

	ustream_free(s);
	close(cl->sfd.fd.fd);
	list_del(&cl->list);
	free(cl);

	/* stop monitoring the file if there are no clients left */
	if (list_empty(&ctx->clients))
		mon_stop(ctx);
}

static void
mon_client_accept(struct mon_context *ctx, int fd)
{
	struct ustream *us;
	struct mon_client *cl;

	cl = calloc(1, sizeof(*cl));
	cl->ctx = ctx;
	us = &cl->sfd.stream;
	us->notify_state = mon_client_notify_state;
	us->string_data = true;
	ustream_fd_init(&cl->sfd, fd);
	list_add_tail(&cl->list, &ctx->clients);
}

static void
mon_server_cb(struct uloop_fd *fd, unsigned int events)
{
	struct mon_context *ctx = container_of(fd, struct mon_context, sfd);
	struct sockaddr_in6 addr;
	unsigned int sl;
	int cfd;

	while (1) {
		sl = sizeof(addr);
		cfd = accept(fd->fd, (struct sockaddr *)&addr, &sl);

		if (cfd < 0) {
			if (errno == EAGAIN)
				return;

			if (errno == EINTR)
				continue;

			uloop_fd_delete(fd);
			close(fd->fd);
		}

		mon_client_accept(ctx, cfd);
	}
}

static int
mon_event_read_buf(struct mon_context *ctx, char *buf)
{
	struct mon_client *cl;
	char *cur, *next;
	int len;

	for (cur = buf; (next = strchr(cur, '\n')); cur = next + 1) {
		*next = 0;

		if (ctx->compression && !list_empty(&ctx->clients))
			zstd_read_fmt(&ctx->buf, "%s\n", cur);
		else
			list_for_each_entry(cl, &ctx->clients, list)
				client_printf(cl, "%s\n", buf);
	}

	len = strlen(cur);
	if (cur > buf)
		memmove(buf, cur, len + 1);

	return len;
}

static void
mon_event_cb(struct uloop_fd *fd, unsigned int events)
{
	struct mon_context *ctx = container_of(fd, struct mon_context, mon_fd);
	char buf[512];
	int len, offset = 0;

	while (1) {
		len = read(fd->fd, buf + offset, sizeof(buf) - 1 - offset);
		if (len < 0) {
			if (errno == EAGAIN)
				return;

			if (errno == EINTR)
				continue;

			mon_stop(ctx);
			return;
		}

		if (!len)
			return;

		buf[offset + len] = 0;
		offset = mon_event_read_buf(ctx, buf);
	}
}

static inline void
mon_flush(struct zstd_buf *buf, const void *data, size_t len)
{
	struct mon_client *cl;
	struct mon_context *ctx = container_of(buf, struct mon_context, buf);

	list_for_each_entry(cl, &ctx->clients, list)
		client_write(cl, data, len);
}

int
rcd_debugfs_monitoring_start(const char *path, int port, size_t bufsize, unsigned int timeout,
                	     bool compression)
{
	int fd, err;
	struct mon_context *ctx;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return errno;

	ctx = calloc(sizeof(*ctx), 1);
	if (!ctx) {
		close(fd);
		return -ENOMEM;
	}

	ctx->compression = compression;
	ctx->port = port;

	if (compression) {
		err = zstd_buf_init(&ctx->buf, bufsize, timeout, mon_flush);
		if (err) {
			close(fd);
			free(ctx);
			return err;
		}
	}
	ctx->mon_fd.fd = fd;
	ctx->mon_fd.cb = mon_event_cb;
	uloop_fd_add(&ctx->mon_fd, ULOOP_READ);

	ctx->sfd.fd = usock(USOCK_SERVER | USOCK_NONBLOCK | USOCK_TCP, "0.0.0.0", usock_port(port));
	if (ctx->sfd.fd < 0) {
		uloop_fd_delete(&ctx->mon_fd);
		close(fd);
		free(ctx);
		return errno;
	}
	ctx->sfd.cb = mon_server_cb;
	uloop_fd_add(&ctx->sfd, ULOOP_READ);

	INIT_LIST_HEAD(&ctx->clients);
	list_add_tail(&ctx->list, &mon_list);

	return 0;
}
