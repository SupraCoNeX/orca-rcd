// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021 Felix Fietkau <nbd@nbd.name> */
#include <libubox/avl-cmp.h>
#include <glob.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <libgen.h>
#include "rcd.h"

static void phy_update(struct vlist_tree *tree, struct vlist_node *node_new,
		       struct vlist_node *node_old);

VLIST_TREE(phy_list, avl_strcmp, phy_update, true, false);

static const char *
phy_file_path(struct phy *phy, const char *file)
{
	static char path[64];

	snprintf(path, sizeof(path), "/sys/kernel/debug/ieee80211/%s/rc/%s", phy_name(phy), file);

	return path;
}

static const char *
phy_debugfs_file_path(struct phy *phy, const char *file)
{
	static char path[128];

	snprintf(path, sizeof(path), "/sys/kernel/debug/ieee80211/%s/%s",
	         phy_name(phy), file);

	return path;
}

static int
phy_event_read_buf(struct phy *phy, char *buf)
{
	char *cur, *next;
	int len;

	for (cur = buf; (next = strchr(cur, '\n')); cur = next + 1) {
		*next = 0;

		rcd_client_phy_event(phy, cur);
#ifdef CONFIG_MQTT
		mqtt_phy_event(phy, cur);
#endif
	}

	len = strlen(cur);
	if (cur > buf)
		memmove(buf, cur, len + 1);

	return len;
}

static void
phy_event_cb(struct uloop_fd *fd, unsigned int events)
{
	struct phy *phy = container_of(fd, struct phy, event_fd);
	char buf[512];
	int len, offset = 0;

	while (1) {
		len = read(fd->fd, buf + offset, sizeof(buf) - 1 - offset);
		if (len < 0) {
			if (errno == EAGAIN)
				return;

			if (errno == EINTR)
				continue;

			vlist_delete(&phy_list, &phy->node);
			return;
		}

		if (!len)
			return;

		buf[offset + len] = 0;
		offset = phy_event_read_buf(phy, buf);
	}
}

static void
phy_init(struct phy *phy)
{
	phy->control_fd = -1;
}

static void
phy_add(struct phy *phy)
{
	int cfd, efd;

	cfd = open(phy_file_path(phy, "api_control"), O_WRONLY);
	if (cfd < 0)
		goto remove;

	efd = open(phy_file_path(phy, "api_event"), O_RDONLY);
	if (efd < 0)
		goto close_cfd;

	phy->control_fd = cfd;
	phy->event_fd.fd = efd;
	phy->event_fd.cb = phy_event_cb;
	uloop_fd_add(&phy->event_fd, ULOOP_READ);

	rcd_client_set_phy_state(NULL, phy, true);
	return;

close_cfd:
	close(cfd);
remove:
	vlist_delete(&phy_list, &phy->node);
}

static void
phy_remove(struct phy *phy)
{
	if (phy->control_fd < 0)
		goto out;

	rcd_client_set_phy_state(NULL, phy, false);
	uloop_fd_delete(&phy->event_fd);
	close(phy->control_fd);
	close(phy->event_fd.fd);

out:
	free(phy);
}

static void
phy_update(struct vlist_tree *tree, struct vlist_node *node_new,
	   struct vlist_node *node_old)
{
	struct phy *phy_new = node_new ? container_of(node_new, struct phy, node) : NULL;
	struct phy *phy_old = node_old ? container_of(node_old, struct phy, node) : NULL;

	if (phy_new && phy_old)
		phy_remove(phy_new);
	else if (phy_new)
		phy_add(phy_new);
	else
		phy_remove(phy_old);
}

static void phy_refresh_timer(struct uloop_timeout *t)
{
	unsigned int i;
	glob_t gl;

	glob("/sys/class/ieee80211/*", 0, NULL, &gl);
	for (i = 0; i < gl.gl_pathc; i++) {
		struct phy *phy;
		char *name, *name_buf;

		name = basename(gl.gl_pathv[i]);
		phy = calloc_a(sizeof(*phy), &name_buf, strlen(name) + 1);
		phy_init(phy);
		vlist_add(&phy_list, &phy->node, strcpy(name_buf, name));
	}
	globfree(&gl);

	uloop_timeout_set(t, 1000);
}

void rcd_phy_init_client(struct client *cl)
{
	struct phy *phy;

	vlist_for_each_element(&phy_list, phy, node)
		rcd_client_set_phy_state(cl, phy, true);
}

void rcd_api_info_dump(struct client *cl, struct phy *phy)
{
	char buf[512];
	FILE *f;
	void *compressed;
	size_t clen;
	int error;

	f = fopen(phy_file_path(phy, "api_info"), "r");
	if (!f)
		return;

	if (cl->compression) {
		while (fgets(buf, sizeof(buf), f) != NULL) {
			error = zstd_fmt_compress(&compressed, &clen, "*;0;%s", buf);
			if (error)
				break;

			client_write(cl, compressed, clen);
			free(compressed);
		}
	} else {
		while (fgets(buf, sizeof(buf), f) != NULL)
			client_printf(cl, "*;0;%s", buf);
	}

	fclose(f);
}

void rcd_phy_info(struct client *cl, struct phy *phy)
{
	char buf[256];
	char caps[3][64] = { "", "", "" };
	char *ty, *value, *res;
	FILE *f;

	f = fopen(phy_file_path(phy, "api_phy"), "r");
	if (!f)
		return;

	/* 
	 * need to stop when we encounter the first sta line. We want to 
	 * print the phy;add line before and then continue with the sta lines.
	 * the API should ensure that there are no other lines after sta lines.
	 */
	while ((res = fgets(buf, sizeof(buf), f)) != NULL && strncmp(buf, "sta", 3)) {
		value = buf;
		ty = strsep(&value, ";");

		if (value)
			value[ strlen(value) - 1 ] = '\0';
                
		if (!strncmp(ty, "drv", 3))
			strncpy(caps[0], value, 64);
		else if (!strncmp(ty, "if", 2))
			strncpy(caps[1], value, 64);
		else if (!strncmp(ty, "tpc", 3))
			strncpy(caps[2], value, 64);
	}

	if (cl->compression)
		client_phy_printf_compressed(cl, phy, "0;add;%s;%s;%s\n",
					     caps[0], caps[1], caps[2]);
	else
		client_phy_printf(cl, phy, "0;add;%s;%s;%s\n", caps[0], caps[1], caps[2]);

	if (!res)
		goto out;

	do {
		value = buf;
		ty = strsep(&value, ";");

		if (strncmp(buf, "sta", 3))
			continue;

		/* Newline character is included so print without an additional one */
		if (cl->compression)
			client_phy_printf_compressed(cl, phy, "0;sta;add;%s", value);
		else
			client_phy_printf(cl, phy, "0;sta;add;%s", value);
	} while (fgets(buf, sizeof(buf), f) != NULL);

out:
	fclose(f);
}

#ifdef CONFIG_MQTT
void mqtt_phy_dump(struct phy *phy, int (*cb)(void*, char*), void *cb_arg)
{
	char buf[512];
	FILE *f;

	f = fopen(phy_file_path(phy, "api_info"), "r");
	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f) != NULL)
		cb(cb_arg, buf);

	fclose(f);
}
#endif

static int
phy_fd_write(int fd, const char *s)
{
retry:
	if (write(fd, s, strlen(s)) < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto retry;

		return errno;
	}

	return 0;
}

static int
phy_debugfs_read(struct client *cl, struct phy *phy, const char *file)
{
	char *buf, *cur;
	int fd, len, err = 0, offset = 0, bufsiz = 512;
	void *cbuf;
	size_t clen;

	fd = open(phy_debugfs_file_path(phy, file), O_RDONLY);
	if (fd < 0)
		return errno;

	buf = malloc(bufsiz);

	while (1) {
		if (!buf) {
			err = -ENOMEM;
			goto error;
		}

		while (1) {
			len = read(fd, buf + offset, bufsiz - 1 - offset);
			if (len == 0) {
				if (buf[offset + len - 1] == '\n')
					buf[offset + len - 1] = '\0';
				else
					buf[offset + len] = '\0';
				goto done;
			} else if (len < 0) {
				if (errno == EAGAIN) {
					err = errno;
					goto error;
				}

				if (errno == EINTR)
					continue;

				free(buf);
				goto error;
			} else if (len == bufsiz - 1 - offset) {
				bufsiz *= 2;
				buf = realloc(buf, bufsiz);
				break;
			}

			offset += len;
		}
	}
done:
	for (cur = buf; *cur != '\0'; cur++)
		if (*cur == '\n')
			*cur = ',';

	if (cl->compression) {
		err = zstd_fmt_compress(&cbuf, &clen, "%s;0;debugfs;%s;%s\n",
		                        phy_name(phy), file, buf);
		if (err) {
			free(buf);
			goto error;
		}

		client_write(cl, cbuf, clen);
		free(cbuf);
	} else {
		client_phy_printf(cl, phy, "0;debugfs;%s;%s\n", file, buf);
	}

	free(buf);
error:
	close(fd);
	return err;
}

static int
phy_debugfs_write(struct phy *phy, const char *file, const char *arg)
{
	int err, fd;

	fd = open(phy_debugfs_file_path(phy, file), O_WRONLY);
	if (fd < 0)
		return errno;

	err = phy_fd_write(fd, arg);
	close(fd);

	return err;
}

static int
__get_args(char **dest, int dest_size, char *str, char *sep)
{
        int i, n;

        for (i = 0, n = 0; i < dest_size; i++) {
                if (!str) {
                        dest[i] = NULL;
                        continue;
                }

                dest[i] = strsep(&str, sep);
                if (dest[i])
                        n++;
        }

        return n;
}

static int
phy_debugfs_monitor(struct client *cl, struct phy *phy, const char *file, char *args)
{
	int nargs, port, err;
	char *argv[3];
	size_t bufsize = 0;
	unsigned int timeout = 0;
	bool compression;

	nargs = __get_args(argv, 3, args, ";");

	if (nargs != 1 && nargs != 3)
		return -EINVAL;

	port = atoi(argv[0]);
	compression = nargs == 3;

	if (compression) {
		bufsize = atoi(argv[1]);
		timeout = atoi(argv[2]);
	}

	err = rcd_debugfs_monitoring_start(phy_debugfs_file_path(phy, file), port, bufsize, timeout,
	                        	   compression);
	if (err)
		return err;

	rcd_client_broadcast("%s;0;debugfs_monitor;%s;%x;%x\n", phy_name(phy), file, port,
	                     compression);
	return 0;
}

static int
phy_debugfs(struct client *cl, struct phy *phy, char *cmd, const char *path, char *args)
{
	/* Make sure path cannot contain dots as this would expose the entire
	 * file system to unauthorized write access if path is something like
	 * '../../../../' ...
	 * Also, limit path length to 64 chars as a precaution.
	 */
	if (!path || strchr(path, '.') || strlen(path) > 64)
		return -EINVAL;

	if (strcmp(cmd, "debugfs") == 0) {
		if (args)
			return phy_debugfs_write(phy, path, args);
		else
			return phy_debugfs_read(cl, phy, path);
	} else if (strcmp(cmd, "debugfs_monitor") == 0) {
		return phy_debugfs_monitor(cl, phy, path, args);
	} else {
		return -EINVAL;
	}

}

void rcd_phy_control(struct client *cl, char *data)
{
	struct phy *phy = NULL;
	const char *err = "Syntax error";
	char *sep, *cmd, *path;
	void *buf;
	size_t clen;
	int error = 0;
	bool wildcard;

	sep = strchr(data, ';');
	if (!sep)
		goto error;

	*sep = 0;
	wildcard = (*data == '*');

	if (!wildcard) {
		phy = vlist_find(&phy_list, data, phy, node);
		if (!phy) {
			err = "PHY not found";
			goto error;
		}
	}

	data = sep + 1;

	sep = strchr(data, ';');
	if (sep) {
		*sep = 0;
		cmd = data;
		if (strncmp(cmd, "debugfs", 7) == 0) {
			if (wildcard) {
				err = "Cannot use debugfs with wildcard phy";
				goto error;
			}
			data = sep + 1;
			path = strsep(&data, ";");
			error = phy_debugfs(cl, phy, cmd, path, data);
			if (error) {
				err = strerror(error);
				goto error;
			}
			return;
		}
		*sep = ';';
	}

	if (wildcard) {
		vlist_for_each_element(&phy_list, phy, node) {
			error = phy_fd_write(phy->control_fd, data);
			if (error) {
				err = strerror(error);
				goto error;
			}
		}
	} else {
		error = phy_fd_write(phy->control_fd, data);
		if (error) {
			err = strerror(error);
			goto error;
		}
	}

	return;

error:
	if (cl->compression) {
		error = zstd_fmt_compress(&buf, &clen, "*;0;#error;%s\n", err);
		if (error)
			return;
		client_write(cl, buf, clen);
		free(buf);
	} else {
		client_printf(cl, "*;0;#error;%s\n", err);
	}
}

void rcd_phy_init(void)
{
	static struct uloop_timeout t = {
		.cb = phy_refresh_timer
	};

	uloop_timeout_set(&t, 1);
}
