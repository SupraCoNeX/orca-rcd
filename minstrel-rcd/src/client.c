// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021 Felix Fietkau <nbd@nbd.name> */
#include <libgen.h>
#include <glob.h>
#include <net/if.h>
#include "rcd.h"

static LIST_HEAD(clients);
static LIST_HEAD(zclients);

static char *
phy_interfaces(struct phy *phy)
{
	glob_t gl;
	char globstr[64], *ifname, *ifaces = NULL;
	size_t len, ifname_ofs, ofs = 0;
	unsigned int i;

	len = snprintf(globstr, sizeof(globstr), "/sys/kernel/debug/ieee80211/%s/netdev:*", phy_name(phy));
	if (len >= sizeof(globstr))
		return NULL;

	ifname_ofs = strchr(globstr, ':') - globstr + 1;

	glob(globstr, 0, NULL, &gl);

	if (gl.gl_pathc == 0)
		goto done;

	ifaces = calloc(IF_NAMESIZE, gl.gl_pathc);
	if (!ifaces)
		goto done;

	for (i = 0; i < gl.gl_pathc; i++) {
		ifname = gl.gl_pathv[i] + ifname_ofs;
		len = strlen(ifname);
		strncpy(ifaces + ofs, ifname, len);

		if (i == gl.gl_pathc - 1)
			break;

		ifaces[ofs + len] = ',';
		ofs += len + 1;
	}

done:
	globfree(&gl);
	return ifaces;
}

static const char *
phy_driver(struct phy *phy)
{
	static char driver[16];
	char path[64], buf[64];
	size_t len;
	ssize_t n_written;

	len = snprintf(path, sizeof(path), "/sys/class/ieee80211/%s/device/driver", phy_name(phy));
	if (len >= sizeof(path))
		return NULL;

	n_written = readlink(path, buf, sizeof(buf));
	if (n_written == sizeof(buf))
		return NULL;

	buf[n_written] = '\0';
	strncpy(driver, basename(buf), sizeof(driver));

	return driver;
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
	void *buf;
	size_t len;
	int err;
	va_list va_args;

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

static void
set_phy_state_compressed(struct client *cl, struct phy *phy, bool add)
{
	char str[64];
	char buf[128]; /* upper bound on compressed size for 16 bytes is 79 */
	size_t clen;
	int error;
	char *ifaces = phy_interfaces(phy);
	if (!ifaces)
		return;

	snprintf(str, sizeof(str), "%s;0;%s;%s;%s\n", phy_name(phy), add ? "add" : "remove",
	         phy_driver(phy), ifaces);

	error = zstd_compress_into(buf, sizeof(buf), str, strlen(str), &clen);
	free(ifaces);
	if (error)
		return;

	client_write(cl, buf, clen);
}

void rcd_client_set_phy_state(struct client *cl, struct phy *phy, bool add)
{
	char *ifaces;

	if (!cl) {
		list_for_each_entry(cl, &clients, list)
			rcd_client_set_phy_state(cl, phy, add);

		list_for_each_entry(cl, &zclients, list)
			rcd_client_set_phy_state(cl, phy, add);
		return;
	}

	if (add && !cl->init_done) {
		rcd_phy_dump(cl, phy);
		cl->init_done = true;
	}

	if (cl->compression) {
		set_phy_state_compressed(cl, phy, add);
	} else {
		ifaces = phy_interfaces(phy);
		if (!ifaces)
			return;

		client_phy_printf(cl, phy, "0;%s;%s;%s\n", add ? "add" : "remove",
		                  phy_driver(phy), ifaces);
		free(ifaces);
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
