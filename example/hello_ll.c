/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file GPL2.txt.
*/

/** @file
 *
 * minimal example filesystem using low-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello_ll.c `pkg-config fuse3 --cflags --libs` -o hello_ll
 *
 * Note: If the pkg-config command fails due to the absence of the fuse3.pc
 *     file, you should configure the path to the fuse3.pc file in the
 *     PKG_CONFIG_PATH variable.
 *
 * ## Source code ##
 * \include hello_ll.c
 */

#define FUSE_USE_VERSION FUSE_MAKE_VERSION(3, 12)

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

static const char *hello_str = "Hello World!\n";
static const char *hello_name = "hello";

static int hello_stat(fuse_ino_t ino, struct stat *stbuf)
{
	stbuf->st_ino = ino;
	switch (ino) {
	case 1:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;

	case 2:
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
		break;

	default:
		return -1;
	}
	return 0;
}

static void hello_ll_init(void *userdata, struct fuse_conn_info *conn)
{
	(void)userdata;

	/* Disable the receiving and processing of FUSE_INTERRUPT requests */
	conn->no_interrupt = 1;

	/* Test setting flags the old way */
	conn->want = FUSE_CAP_ASYNC_READ;
	conn->want &= ~FUSE_CAP_ASYNC_READ;
}

static void hello_ll_getattr(fuse_req_t req, fuse_ino_t ino,
			     struct fuse_file_info *fi)
{
	struct stat stbuf;

	(void) fi;

	memset(&stbuf, 0, sizeof(stbuf));
	if (hello_stat(ino, &stbuf) == -1)
		fuse_reply_err(req, ENOENT);
	else
		fuse_reply_attr(req, &stbuf, 1.0);
}

static void hello_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;

	if (parent != 1 || strcmp(name, hello_name) != 0)
		fuse_reply_err(req, ENOENT);
	else {
		memset(&e, 0, sizeof(e));
		e.ino = 2;
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		hello_stat(e.ino, &e.attr);

		fuse_reply_entry(req, &e);
	}
}

struct dirbuf {
	char *p;
	size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
		       fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
			  b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
			     off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off,
				      min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

static void hello_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			     off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	if (ino != 1)
		fuse_reply_err(req, ENOTDIR);
	else {
		struct dirbuf b;

		memset(&b, 0, sizeof(b));
		dirbuf_add(req, &b, ".", 1);
		dirbuf_add(req, &b, "..", 1);
		dirbuf_add(req, &b, hello_name, 2);
		reply_buf_limited(req, b.p, b.size, off, size);
		free(b.p);
	}
}

static void hello_ll_open(fuse_req_t req, fuse_ino_t ino,
			  struct fuse_file_info *fi)
{
	if (ino != 2)
		fuse_reply_err(req, EISDIR);
	else if ((fi->flags & O_ACCMODE) != O_RDONLY)
		fuse_reply_err(req, EACCES);
	else
		fuse_reply_open(req, fi);
}

static void hello_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	assert(ino == 2);
	reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}

static void hello_ll_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
							  size_t size)
{
	(void)size;
	assert(ino == 1 || ino == 2);
	if (strcmp(name, "hello_ll_getxattr_name") == 0)
	{
		const char *buf = "hello_ll_getxattr_value";
		fuse_reply_buf(req, buf, strlen(buf));
	}
	else
	{
		fuse_reply_err(req, ENOTSUP);
	}
}

static void hello_ll_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
							  const char *value, size_t size, int flags)
{
	(void)flags;
	(void)size;
	assert(ino == 1 || ino == 2);
	const char* exp_val = "hello_ll_setxattr_value";
	if (strcmp(name, "hello_ll_setxattr_name") == 0 &&
	    strlen(exp_val) == size &&
	    strncmp(value, exp_val, size) == 0)
	{
		fuse_reply_err(req, 0);
	}
	else
	{
		fuse_reply_err(req, ENOTSUP);
	}
}

static void hello_ll_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	assert(ino == 1 || ino == 2);
	if (strcmp(name, "hello_ll_removexattr_name") == 0)
	{
		fuse_reply_err(req, 0);
	}
	else
	{
		fuse_reply_err(req, ENOTSUP);
	}
}

static const struct fuse_lowlevel_ops hello_ll_oper = {
	.init = hello_ll_init,
	.lookup = hello_ll_lookup,
	.getattr = hello_ll_getattr,
	.readdir = hello_ll_readdir,
	.open = hello_ll_open,
	.read = hello_ll_read,
	.setxattr = hello_ll_setxattr,
	.getxattr = hello_ll_getxattr,
	.removexattr = hello_ll_removexattr,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	struct fuse_loop_config *config;
	int ret = -1;

	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;
	if (opts.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		ret = 0;
		goto err_out1;
	} else if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		ret = 0;
		goto err_out1;
	}

	if(opts.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		printf("       %s --help\n", argv[0]);
		ret = 1;
		goto err_out1;
	}

	se = fuse_session_new(&args, &hello_ll_oper,
			      sizeof(hello_ll_oper), NULL);
	if (se == NULL)
	    goto err_out1;

	if (fuse_set_signal_handlers(se) != 0)
	    goto err_out2;

	if (fuse_session_mount(se, opts.mountpoint) != 0)
	    goto err_out3;

	fuse_daemonize(opts.foreground);

	/* Block until ctrl+c or fusermount -u */
	if (opts.singlethread)
		ret = fuse_session_loop(se);
	else {
		config = fuse_loop_cfg_create();
		fuse_loop_cfg_set_clone_fd(config, opts.clone_fd);
		fuse_loop_cfg_set_max_threads(config, opts.max_threads);
		ret = fuse_session_loop_mt(se, config);
		fuse_loop_cfg_destroy(config);
		config = NULL;
	}

	fuse_session_unmount(se);
err_out3:
	fuse_remove_signal_handlers(se);
err_out2:
	fuse_session_destroy(se);
err_out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return ret ? 1 : 0;
}
