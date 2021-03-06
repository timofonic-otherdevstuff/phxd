#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "main.h"
#include "xmalloc.h"

#define MAX_NEWS_SIZE	0xffffffff

static char *__news_buf = 0;
static size_t __news_len = 0;

static char *
read_file (int fd, size_t max, size_t *lenp)
{
#define BLOCKSIZE	4096
	char *buf = __news_buf;
	size_t len = __news_len;
	size_t pos = 0;
	size_t rn;
	ssize_t r;

	for (;;) {
		if (pos+BLOCKSIZE > len) {
			len += BLOCKSIZE;
			buf = xrealloc(buf, len);
		}
		rn = max > BLOCKSIZE ? BLOCKSIZE : max;
		r = read(fd, buf+pos, rn);
		if (r <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			break;
		}
		pos += r;
		max -= r;
		if (r != (ssize_t)rn || !max)
			break;
	}
	if (lenp)
		*lenp = pos;
	__news_buf = buf;
	__news_len = len;

	return buf;
}

void
news_send_file (struct htlc_conn *htlc)
{
	char *buf;
	size_t len;
	int fd;

	if ((fd = open(htlc->newsfile, O_RDONLY)) < 0) {
		snd_strerror(htlc, errno);
		return;
	}
	buf = read_file(fd, MAX_NEWS_SIZE, &len);
	if (!buf) {
		snd_strerror(htlc, errno);
	} else {
		LF2CR(buf, len);
		hlwrite(htlc, HTLS_HDR_TASK, 0, 1, HTLS_DATA_NEWS, len, buf);
	}
	close(fd);
}

static void
write_file (int fd, const u_int8_t *buf, size_t len)
{
	ssize_t r;
	size_t pos = 0;

	for (;;) {
		r = write(fd, buf+pos, len);
		if (r <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			break;
		}
		pos += r;
		len -= r;
		if (!len)
			break;
	}
}

void
news_save_post (char *newsfile, u_int8_t *post, u_int16_t postlen)
{
	char *buf;
	size_t len;
	int fd;

	if ((fd = open(newsfile, O_RDWR|O_CREAT, hxd_cfg.permissions.news_files)) < 0)
		return;
	buf = read_file(fd, MAX_NEWS_SIZE, &len);
	lseek(fd, 0, SEEK_SET);
	write_file(fd, post, postlen);
	if (buf)
		write_file(fd, buf, len);
	fsync(fd);
	close(fd);
}

void
agreement_send_file (struct htlc_conn *htlc)
{
	char *buf;
	size_t len;
	int fd;

	if ((fd = open(hxd_cfg.paths.agreement, O_RDONLY)) < 0)
		return;
	buf = read_file(fd, MAX_NEWS_SIZE, &len);
	if (buf) {
		LF2CR(buf, len);
		hlwrite(htlc, HTLS_HDR_AGREEMENT, 0, 1, HTLS_DATA_AGREEMENT, len, buf);
	}

	close(fd);
}
