// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021 Felix Fietkau <nbd@nbd.name> */
#include <libgen.h>
#include <glob.h>
#include <net/if.h>
#include "rcd.h"

static LIST_HEAD(clients);
static LIST_HEAD(zclients);

int client_vprintf_compressed(struct client *cl, const char *fmt, va_list va_args) {
	void *compressed;
	size_t clen;
	int error;

	error = zstd_fmt_compress_va(&compressed, &clen, fmt, va_args);
	if (error)
		return error;

	client_write(cl, compressed, clen);
	free(compressed);
	return 0;
}

int client_vprintf(struct client *cl, const char *fmt, va_list va_args) {
	int res = 0;

	if (cl->compression)
		res = client_vprintf_compressed(cl, fmt, va_args);
	else
		ustream_vprintf(&(cl)->sfd.stream, fmt, va_args);

	return res;
}

int client_printf(struct client *cl, const char *fmt, ...) {
	va_list va_args;
	int res;

	va_start(va_args, fmt);
	res = client_vprintf(cl, fmt, va_args);
	va_end(va_args);

	return res;
}

void rcd_client_phy_event(struct phy *phy, const char *str)
{
	struct client *cl;

	list_for_each_entry(cl, &clients, list)
		client_phy_printf(cl, phy, "%s\n", str);

	/* only fill the input buffer if there are connected clients */
	if (rcd_has_clients(true))
		zstd_read_fmt(NULL, "%s;%s\n", phy_name(phy), str);
}

void rcd_client_broadcast(const char *fmt, ...)
{
	struct client *cl;
	va_list va_args;
	void *buf;
	size_t len;
	int err;

	va_start(va_args, fmt);

	list_for_each_entry (cl, &clients, list)
		client_vprintf(cl, fmt, va_args);

	if (!list_empty(&zclients)) {
		err = zstd_fmt_compress_va(&buf, &len, fmt, va_args);
		if (err)
			goto out;

		list_for_each_entry(cl, &zclients, list)
			client_write(cl, buf, len);

		free(buf);
	}

out:
	va_end(va_args);
}

void rcd_client_set_phy_state(struct client *cl, struct phy *phy, bool add)
{
	if (!cl) {
		list_for_each_entry(cl, &clients, list)
			rcd_client_set_phy_state(cl, phy, add);

		list_for_each_entry(cl, &zclients, list)
			rcd_client_set_phy_state(cl, phy, add);
		return;
	}

	if (add) {
		if (!cl->init_done) {
			rcd_api_info_dump(cl, phy);
			cl->init_done = true;
		}

		rcd_phy_info(cl, phy);
	} else {
		client_phy_printf(cl, phy, "0;remove\n");
	}
}

static void
client_start(struct client *cl)
{
	struct phy *phy;

	vlist_for_each_element(&phy_list, phy, node)
		rcd_client_set_phy_state(cl, phy, true);
}

static int
client_handle_data(struct client *cl, char *data)
{
	char *sep;
	int len = 0;

	while ((sep = strchr(data, '\n')) != NULL) {
		len += sep - data + 1;
		if (sep[-1] == '\r')
			sep[-1] = 0;
		*sep = 0;
		if (data != sep)
			rcd_phy_control(cl, data);

		data = sep + 1;
	}

	return len;
}

static void
client_notify_read(struct ustream *s, int bytes)
{
	struct client *cl = container_of(s, struct client, sfd.stream);
	char *data;
	int len;

	while (1) {
		data = ustream_get_read_buf(s, &len);
		if (!data)
			return;

		len = client_handle_data(cl, data);
		if (!len)
			return;

		ustream_consume(s, len);
	}
}

static void
client_notify_state(struct ustream *s)
{
	struct client *cl = container_of(s, struct client, sfd.stream);

	if (!s->write_error && !s->eof)
		return;

	ustream_free(s);
	close(cl->sfd.fd.fd);
	list_del(&cl->list);
	free(cl);
}

void rcd_client_accept(int fd, bool compression)
{
	struct ustream *us;
	struct client *cl;

	cl = calloc(1, sizeof(*cl));
	cl->compression = compression;
	us = &cl->sfd.stream;
	us->notify_read = client_notify_read;
	us->notify_state = client_notify_state;
	us->string_data = true;
	ustream_fd_init(&cl->sfd, fd);
	list_add_tail(&cl->list, compression ? &zclients : &clients);
	client_start(cl);
}

bool
rcd_has_clients(bool compression)
{
	return compression ? !list_empty(&zclients) : !list_empty(&clients);
}

#ifdef CONFIG_ZSTD
void rcd_client_write(const void *buf, size_t len, bool compressed)
{
	struct client *cl;
	struct list_head *head = compressed ? &zclients : &clients;

	list_for_each_entry(cl, head, list)
		client_write(cl, buf, len);
}
#endif
