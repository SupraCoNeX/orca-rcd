#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>

#include <zstd.h>

#include "rcd.h"

static ZSTD_CDict *_dict = NULL;
static ZSTD_CCtx *_ctx = NULL;

static struct zstd_buf *default_buf;

static const char *EMSG_NODICT = "no dictionary provided";
static const char *EMSG_LOADFAILED = "error loading dictionary";
static const char *EMSG_NOCTX = "error creating zstd context";
static const char *EMSG_BUFALLOC = "error allocating buffers";

static inline void
reset_timeout(struct zstd_buf *buf)
{
	uloop_timeout_set(&buf->timeout, buf->timeout_ms);
}

static int
__compress(void *data, size_t len, void *dst, size_t dstlen, size_t *complen)
{
	size_t clen = ZSTD_compress_usingCDict(_ctx, dst, dstlen, data, len, _dict);
	if (ZSTD_isError(clen))
		goto error;

	*complen = clen;
	return 0;

error:
	fprintf(stderr, "compression failed: %s\n", ZSTD_getErrorName(clen));
	*complen = 0;
	return -1;
}

int
zstd_compress(void *data, size_t len, void **buf, size_t *buflen)
{
	size_t clen, dstlen = ZSTD_compressBound(len);
	int error;
	void *dst = malloc(dstlen);
	if (!dst)
		goto error;

	error = __compress(data, len, dst, dstlen, &clen);
	if (error)
		goto free;

	*buf = dst;
	*buflen = clen;
	return 0;

free:
	free(dst);
error:
	*buf = NULL;
	*buflen = 0;
	return -1;
}

int
zstd_compress_into(void *dst, size_t dstlen, void *data, size_t len, size_t *complen)
{
	size_t clen = ZSTD_compressBound(len);

	if (clen > dstlen) {
		fprintf(stderr, "cannot reliably compress data of size %zu (compress bound %zu) into buffer of size %zu\n", len, clen, dstlen);
		return -1;
	}

	return __compress(data, len, dst, dstlen, complen);
}

int
zstd_fmt_compress_va(void **buf, size_t *buflen, const char *fmt, va_list va_args)
{
	char *str, *tmp;
	size_t slen = 1024, n;
	int error;

	str = malloc(slen);
	if (!str)
		return -ENOMEM;

	do {
		n = vsnprintf(str, slen, fmt, va_args);

		if (n >= slen) {
			/* grow buffer exponentially */
			slen *= 2;
			tmp = realloc(str, slen);
			if (!tmp) {
				error = -ENOMEM;
				goto error;
			}

			str = tmp;
			continue;
		}
	} while (0);

	error = zstd_compress(str, strlen(str) + 1, buf, buflen);
	free(str);

	return error;

error:
	free(str);
	*buf = NULL;
	*buflen = 0;
	return error;
}

int
zstd_fmt_compress(void **buf, size_t *buflen, const char *fmt, ...)
{
	va_list va_args;
	int err;

	va_start(va_args, fmt);
	err = zstd_fmt_compress_va(buf, buflen, fmt, va_args);
	va_end(va_args);

	return err;
}

static ZSTD_CDict*
load_dict(const char *path, int complvl)
{
	struct stat st;
	size_t size, read_size;
	off_t fsize;
	void *buf;
	FILE *file;
	ZSTD_CDict *dict;

	if (stat(path, &st)) {
		perror(path);
		return NULL;
	}

	fsize = st.st_size;
	size = (size_t) fsize;

	if ((fsize < 0) || (fsize != (off_t) size)) {
		fprintf(stderr, "%s: filesize too large\n", path);
		return NULL;
	}

	buf = malloc(size);
	if (!buf) {
		perror("malloc");
		return NULL;
	}

	file = fopen(path, "rb");
	if (!file) {
		perror(path);
		return NULL;
	}

	read_size = fread(buf, 1, size, file);
	if (read_size != size) {
		fprintf(stderr, "fread: %s : %s\n", path, strerror(errno));
		return NULL;
	}

	fclose(file);

	dict = ZSTD_createCDict(buf, size, complvl);
	free(buf);

	return dict;
}

static void
zstd_compress_and_flush(struct zstd_buf *buf)
{
	size_t remaining;
	bool done;
	ZSTD_inBuffer in;
	ZSTD_outBuffer out;

	if (buf->in.pos == 0)
		goto done;

	in.src = buf->in.buf;
	in.size = buf->in.pos;
	in.pos = 0;

	do {
		out.dst = buf->out.buf;
		out.size = buf->out.size;
		out.pos = 0;

		remaining = ZSTD_compressStream2(_ctx, &out, &in, ZSTD_e_end);
		if (ZSTD_isError(remaining)) {
			fprintf(stderr, "stream compression error: %s\n", ZSTD_getErrorName(remaining));
			buf->in.pos = 0;
			goto done;
		}

		buf->flush(buf, out.dst, out.pos);

		done = remaining == 0;
	} while (!done);

	buf->in.pos = 0;

done:
	reset_timeout(buf);
}

int
zstd_read_fmt(struct zstd_buf *buf, const char *fmt, ...)
{
	if (!buf)
		buf = default_buf;

	size_t read, remaining;
	va_list va_args;

	if (buf == NULL)
		buf = default_buf;

	remaining = buf->in.size - buf->in.pos;

	va_start(va_args, fmt);
	read = vsnprintf(buf->in.buf + buf->in.pos, remaining, fmt, va_args);
	va_end(va_args);

	if (read < remaining)
		goto ok;

	zstd_compress_and_flush(buf);

	va_start(va_args, fmt);
	read = vsnprintf(buf->in.buf, buf->in.size, fmt, va_args);
	va_end(va_args);

	if (read < buf->in.size)
		goto ok;

	fprintf(stderr, "discarding data of length %zu: cannot fit into buffer of size: %zu\n",
	        read, buf->in.size);

	return -1;

ok:
	buf->in.pos += read;
	if (!buf->timeout.pending)
		reset_timeout(buf);

	return 0;
}

static inline void
timeout_flush(struct uloop_timeout *t)
{
	struct zstd_buf *buf = container_of(t, struct zstd_buf, timeout);
	zstd_compress_and_flush(buf);
}

int
zstd_buf_init(struct zstd_buf *buf, size_t size, unsigned int timeout_ms, zstd_buf_flush_cb cb)
{
	memset(buf, 0, sizeof(*buf));
	INIT_LIST_HEAD(&buf->timeout.list);

	buf->in.size = size;
	buf->out.size = ZSTD_compressBound(size);

	buf->in.buf = calloc_a(size, &buf->out.buf, buf->out.size);
	if (!buf->in.buf)
		return -ENOMEM;

	buf->timeout_ms = timeout_ms;
	buf->timeout.cb = timeout_flush;
	buf->flush = cb;
	return 0;
}

static inline void
default_flush(struct zstd_buf *buf, const void *data, size_t len)
{
	rcd_client_write(data, len, true);
}

int
zstd_init(struct zstd_buf *buf, const struct zstd_opts *o)
{
	const char *errmsg = NULL;

	if (!o->dict) {
		errmsg = EMSG_NODICT;
		goto error;
	}

	_dict = load_dict(o->dict, o->comp_level);
	if (!_dict) {
		errmsg = EMSG_LOADFAILED;
		goto error;
	}

	_ctx = ZSTD_createCCtx();
	if (!_ctx) {
		errmsg = EMSG_NOCTX;
		goto free;
	}

	ZSTD_CCtx_refCDict(_ctx, _dict);

	if (zstd_buf_init(buf, o->bufsize, o->timeout_ms, default_flush)) {
		errmsg = EMSG_BUFALLOC;
		goto free;
	}

	default_buf = buf;

	return 0;
free:
	ZSTD_freeCDict(_dict);
error:
	fprintf(stderr, "Could not initialize zstd compression: %s\n", errmsg);
	return -1;
}

void
zstd_stop(bool flush)
{
	if (flush)
		zstd_compress_and_flush(default_buf);

	free(default_buf->in.buf);
	ZSTD_freeCDict(_dict);
}

