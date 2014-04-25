/*	pa4-encfs.c
*	
*	Sahle Alturaigi
*	Last updated: 4/25/14
*	
*	File system implementation for CSCI 3753-PA4
*	A lot of code was adapted from: http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/ 
*	tutorials.
*	Will put more detail as project continues...
*
*	To-Do:
*		1.) DONE. File system properly mirrors target directory specified at mount time.
*		2.) File system uses extended attributes to differentiate between encrypted and unencrypted files
*		3.) File system can transparently read and write securely encrypted files using a pass phrase
*			specified at mount time.
*		4.) File system can transparently read and updated unencrypted files
*		5.) Add proper comments to all of code
*		6.) OPTIONAL: Refer to handout
*
*/

// Taken from fusexmp.c
#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

// Taken from xattr-util.c
#ifdef linux
#define ENOATTR ENODATA 	// Linux is missing ENOATTR error, using ENODATA instead
#define _XOPEN_SOURCE 700 	// For pread(), pwrite(), and open_memstream
#endif

#define XMP_DATA ((struct xmp_state *) fuse_get_context()->private_data)

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Libraries */
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/types.h>
#include <limits.h>
#include "aes-crypt.h" // For do_crypt()

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


/* Struct used mostly in main to hold user passed info. */
struct xmp_state {
	char* mirror_dir;
	char* key_phrase;
};

/* 
*	Function to change paths from root directory to a specific mirror directory.
*	Gets the mirror directory and puts it onto fpath. 
*	Taken from bbfs.c 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
*/
static void xmp_fullpath(char fpath[PATH_MAX], const char *path)
{
	strcpy(fpath, XMP_DATA->mirror_dir);
	strncat(fpath, path, PATH_MAX);
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = lstat(fpath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = access(fpath, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = readlink(fpath, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	dp = opendir(fpath);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(fpath, mode);
	else
		res = mknod(fpath, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = mkdir(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = unlink(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = rmdir(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

/* Modified to incorporate full path of from and to. */
static int xmp_symlink(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];

	xmp_fullpath(ffrom, from);
	xmp_fullpath(fto, to);

	res = symlink(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

/* Modified to incorporate full path of from and to. */
static int xmp_rename(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];

	xmp_fullpath(ffrom, from);
	xmp_fullpath(fto, to);

	res = rename(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

/* Modified to incorporate full path of from and to. */
static int xmp_link(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];

	xmp_fullpath(ffrom, from);
	xmp_fullpath(fto, to);

	res = link(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = chmod(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = lchown(fpath, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = truncate(fpath, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(fpath, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

/// Need a lot of work
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	char fpath[PATH_MAX];
	(void) fi;
	// int crypt_action = PASS_THROUGH;
	// ssize_t valsize = 0;
	// char *tmpval = NULL;

	xmp_fullpath(fpath, path);

	fd = open(fpath, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

/// Need a lot of work
static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	fd = open(fpath, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = statvfs(fpath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

/// Need some work
static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) {

    (void) fi;

    int res;
    char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

    res = creat(fpath, mode);
    if(res == -1)
	return -errno;

    close(res);

    return 0;
}


static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = lsetxattr(fpath, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);
	res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	int res;
	char fpath[PATH_MAX];

	xmp_fullpath(fpath, path);

	res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */



/// Comment later
static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.utimens	= xmp_utimens,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.create     = xmp_create,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	struct xmp_state *xmp_data; // Temp for holding mirror directory and key phrase

	umask(0); // Get or set the file mode creation mask. umask(0) = read,write,execute permission

	/* Check if we have a proper amount of args. */
	if(argc < 4)
	{
		fprintf(stderr, "Must be in the format: ./pa4-encfs <passphrase> <mirror_directory> <mount_point>\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize xmp_data to hold xmp_state information */
	xmp_data = malloc(sizeof(struct xmp_state));
	if(xmp_data == NULL)
	{
		fprintf(stderr, "Error in allocating memory for xmp_data (In int main). Exiting...\n");
		exit(EXIT_FAILURE);
	}

	/* Extracting user specified data into xmp_data */
	(*xmp_data).mirror_dir = realpath(argv[2], NULL);	// Mirror directory
	(*xmp_data).key_phrase = argv[1];					// Pass key for encryption/decryption

	/* FOR DEBUG. Display what has been stored in xmp_data */
	fprintf(stdout, "mirror_dir = %s\n", (*xmp_data).mirror_dir);
	fprintf(stdout, "key_phrase = %s\n", (*xmp_data).key_phrase);

	/// May change this...
	argv[1] = argv[3]; 	// Mount directory
	argv[2] = argv[4];	// Flag
	argv[3] = NULL;
	argv[4] = NULL;
	argc = argc - 2;	// Have argc match with argv

	/* 
	*	int fuse_main(int argc, char* argv[], const struct fuse_operations *op, void *user_data)
	*	@param op 			= the file system operation
	*	@param user_data 	= user data supplied in the context during the init() method
	*	Should return 0 if successful.
	*/
	return fuse_main(argc, argv, &xmp_oper, xmp_data);
}