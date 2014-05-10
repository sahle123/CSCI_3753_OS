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
*		2.) DONE. File system uses extended attributes to differentiate between encrypted and unencrypted files
*		3.) File system can transparently read and write securely encrypted files using a pass phrase
*			specified at mount time.
*		4.) File system can transparently read and updated unencrypted files
*		5.) Add proper comments to all of code
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

/* do_crypt actions as per the aes-crypt.h convention */
#define ENCRYPT 1
#define DECRYPT 0
#define PASS_THROUGH -1

/* Definitions of extended attribute name and values */
#define XATRR_ENCRYPTED_FLAG 	"user.pa4-encfs.encrypted"
#define ENCRYPTED 				"true"
#define UNENCRYPTED 			"false"

/* File suffixes for the tmp_path function to distinguish which function is calling it */
#define SUFFIXREAD 		".read"
#define SUFFIXWRITE 	".write"
#define SUFFIXCREATE 	".create"
#define SUFFIXGETATTR 	".getattr"

/* Taken from bbfs.c 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu> */
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

/* 
*	This is function that creates a physical, temporary file
*	that has the length of the path and file type suffix
*	as one single string. 
*	Credit to Alex Beal for this function
*/
char* tmp_path(const char* old_path, const char *suffix)
{
    char* new_path;
    int len = 0;

    // Find the length of the old path and file type (e.g. .read)
    len = strlen(old_path) + strlen(suffix) + 1;
    
    new_path = malloc(sizeof(char)*len);
    
    if(new_path == NULL)
        return NULL;

    new_path[0] = '\0';

    strcat(new_path, old_path);
    strcat(new_path, suffix);

    return new_path;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;
	int crypt_action = PASS_THROUGH;
	ssize_t valsize = 0;
	char fpath[PATH_MAX];
	char* tmpval = 0;

	/* 
	*	Taken and adapted from: http://linux.about.com/library/cmd/blcmdl2_lstat.htm 
	*	A bunch of local variables to store the file attributes that will be 
	*	extracted from lstat().
	*/
	time_t    atime;   	/* time of last access */
    time_t    mtime;   	/* time of last modification */
    time_t    tctime;   /* time of last status change */
    dev_t     t_dev;    /* ID of device containing file */
    ino_t     t_ino;    /* I-node number */
    mode_t    mode;    	/* protection */
    nlink_t   t_nlink;  /* number of hard links */
    uid_t     t_uid;    /* user ID of owner */
    gid_t     t_gid;    /* group ID of owner */
    dev_t     t_rdev;   /* device ID (if special file) */

	xmp_fullpath(fpath, path);

	res = lstat(fpath, stbuf);	// Get file status

	if (res == -1)
		return -errno;

	/* Check to see if this is a regular file. */
	if (S_ISREG(stbuf->st_mode))
	{

		/* 
		*	We do not want to change these file attributes.
		*	Storing them in a bunch of local temps.
		*/
		atime = stbuf->st_atime;
		mtime = stbuf->st_mtime;
		tctime = stbuf->st_ctime;
		t_dev = stbuf->st_dev;
		t_ino = stbuf->st_ino;
		mode = stbuf->st_mode;
		t_nlink = stbuf->st_nlink;
		t_uid = stbuf->st_uid;
		t_gid = stbuf->st_gid;
		t_rdev = stbuf->st_rdev;


		/* Get size of flag value and value itself */
		valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
		tmpval = malloc(sizeof(*tmpval)*(valsize));
		valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, tmpval, valsize);
		
		fprintf(stderr, "GET_ATTR: Xattr Value: %s\n", tmpval);

		/* If the specified attribute doesn't exist or it's set to false */
		if (valsize < 0 || memcmp(tmpval, UNENCRYPTED, 5) == 0)
		{
			if(errno == ENOATTR)
				fprintf(stderr, "GET_ATTR: No %s attribute set\n", XATRR_ENCRYPTED_FLAG);

			fprintf(stderr, "GET_ATTR: file is unencrypted, leaving crypt_action as PASS_THROUGH\n");
		} 

		/* If the attribute exists and is true then we need to get size of decrypted file */
		else if (memcmp(tmpval, ENCRYPTED, 4) == 0)
		{
			fprintf(stderr, "GET_ATTR: file is encrypted, need to decrypt\n");
			crypt_action = DECRYPT;
		}

		const char *tmpPath = tmp_path(fpath, SUFFIXGETATTR);
		FILE *tmpFile = fopen(tmpPath, "wb+");
		FILE *f = fopen(fpath, "rb");

		fprintf(stderr, "GET_ATTR: fpath: %s\nGET_ATTR: tmpPath: %s\n", fpath, tmpPath);

		if(!do_crypt(f, tmpFile, crypt_action, XMP_DATA->key_phrase))
			fprintf(stderr, "GET_ATTR: do_crypt failed\n");

		fclose(f);
		fclose(tmpFile);

		/* Get size of decrypted file and store in stat struct */
		res = lstat(tmpPath, stbuf);
		if (res == -1)
			return -errno;

		/* Put info about file we did not want to change back into stat struct*/
		stbuf->st_atime = atime;
		stbuf->st_mtime = mtime;
		stbuf->st_ctime = tctime;
		stbuf->st_dev = t_dev;
		stbuf->st_ino = t_ino;
		stbuf->st_mode = mode;
		stbuf->st_nlink = t_nlink;
		stbuf->st_uid = t_uid;
		stbuf->st_gid = t_gid;
		stbuf->st_rdev = t_rdev;

		free(tmpval);
		remove(tmpPath);	// Remove file
	}
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

	res = open(fpath, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

/* Decryption happens here. */
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{

	int res;
	char fpath[PATH_MAX];
	(void) fi;
	
	int crypt_action = PASS_THROUGH;	// Current encryption action. Will change.
	ssize_t valsize = 0;
	char* tmpval = NULL;
	const char* tmpPath;

	xmp_fullpath(fpath, path);

	/* Get size of flag value and value itself */
	valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
	tmpval = malloc(sizeof(*tmpval)*(valsize));
	valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, tmpval, valsize);

	fprintf(stderr, "READ: xattr value: %s\n", tmpval);

	/* If the specified attribute doesn't exist or it's set to false */
	if(valsize < 0 || memcmp(tmpval, "false", 5) == 0)
	{
		if(errno == ENOATTR)
			fprintf(stderr, "READ: No %s attribute set\n", XATRR_ENCRYPTED_FLAG);

		fprintf(stderr, "READ: file is unencrypted, leaving crypt_action as PASS_THROUGH\n");
	}
	/* else if the tmpval is set to true, then decrypt */
	else if (memcmp(tmpval, "true", 4) == 0)
	{
		fprintf(stderr, "READ: file is encrypted, need to decrypt\n");
		crypt_action = DECRYPT;
	}

	tmpPath = tmp_path(fpath, SUFFIXREAD);
	FILE* tmpFile = fopen(tmpPath, "wb+");
	FILE* f = fopen(fpath, "rb");

	fprintf(stderr, "READ: fpath: %s\ntmpPath: %s\n", fpath, tmpPath);

	if(!do_crypt(f, tmpFile, crypt_action, XMP_DATA->key_phrase))
	{
		fprintf(stderr, "READ: do_crypt failed\n");
	}

	fseek(tmpFile, 0, SEEK_END);
	size_t tmpFilelen = ftell(tmpFile);
	fseek(tmpFile, 0, SEEK_SET);

	fprintf(stderr, "READ: size given by read: %zu\nsize of tmpFile: %zu\nsize of offset: %zu\n", size, tmpFilelen, offset);

	res = fread(buf, 1, tmpFilelen, tmpFile);
	if (res == -1)
		res = -errno;

	fclose(f);
	fclose(tmpFile);
	remove(tmpPath);
	free(tmpval);

	return res;

/*
	char fpath[PATH_MAX];
	xmp_fullpath(fpath, path);

	//int fd;
	int res;

	(void) fi;
	(void) offset;

	char *mtext;
	size_t msize;
	int action = PASS_THROUGH;
	char xattr_value[8];
	ssize_t xattr_len;

	FILE *inFile, *outFile;

	inFile = fopen(path, "r");
	if (inFile == NULL)
		return -errno;
	// open file from memory - The Heap
	outFile = open_memstream(&mtext, &msize);
	if (outFile == NULL)
		return -errno;

	xattr_len = getxattr(path, XATRR_ENCRYPTED_FLAG, xattr_value, 8);
	if (xattr_len != -1 && !memcmp(xattr_value, ENCRYPTED, 4))
		action = DECRYPT;

	do_crypt(inFile, outFile, action, XMP_DATA->key_phrase);
	fclose(inFile);

	fflush(outFile);
	fseek(outFile, offset, SEEK_SET);
	res = fread(buf, 1, size, outFile);

	if (res == -1)
		res = -errno;

	//close(fd);
	fclose(outFile);
	return res;
	*/
}

/* Encryption and decryption happens here */
static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	(void) offset;
	int fd;
	int res;
	int crypt_action = PASS_THROUGH;
	ssize_t valsize = 0;
	char fpath[PATH_MAX];
	char* tmpval = NULL;

	xmp_fullpath(fpath, path);

	valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
	tmpval = malloc(sizeof(*tmpval)*(valsize));
	valsize = getxattr(fpath, XATRR_ENCRYPTED_FLAG, tmpval, valsize);

	fprintf(stderr, " WRITE: Xattr Value: %s\n", tmpval);

	if (valsize < 0 || memcmp(tmpval, "false", 5) == 0)
	{
		if(errno == ENOATTR)
			fprintf(stderr, "WRITE: No %s attribute set\n", XATRR_ENCRYPTED_FLAG);
	
		fprintf(stderr, "WRITE: file is unencrypted, leaving crypt_action as pass-through\n");

	}

	/* If the attribute exists and is true then we need to get size of decrypted file */
	else if (memcmp(tmpval, "true", 4) == 0)
	{
		fprintf(stderr, "WRITE: file is encrypted, need to decrypt\n");
		crypt_action = DECRYPT;
	}

	fprintf(stderr, "WRITE: crypt_action is set to %d\n", crypt_action);

	/* If the file to be written to is encrypted */
	if (crypt_action == DECRYPT)
	{
		fprintf(stderr, "WRITE: File to be written is encrypted\n");
		
		FILE *f = fopen(fpath, "rb+");
		const char *tmpPath = tmp_path(fpath, SUFFIXWRITE);
		FILE *tmpFile = fopen(tmpPath, "wb+");

		fprintf(stderr, "WRITE: path of original file %s\n", fpath);

		fseek(f, 0, SEEK_END);
		size_t original = ftell(f);
		fseek(f, 0, SEEK_SET);
		fprintf(stderr, "WRITE: Size of original file %zu\n", original);

		fprintf(stderr, "WRITE: Decrypting contents of original file to tmpFile for writing\n");
		if(!do_crypt(f, tmpFile, DECRYPT, XMP_DATA->key_phrase))
			fprintf(stderr, "WRITE: do_crypt failed\n");

    	fseek(f, 0, SEEK_SET);

    	size_t tmpFilelen = ftell(tmpFile);
    	fprintf(stderr, "WRITE: Size to be written to tmpFile %zu\n", size);
    	fprintf(stderr, "WRITE: size of tmpFile %zu\n", tmpFilelen);
    	fprintf(stderr, "WRITE: Writing to tmpFile\n");

    	res = fwrite(buf, 1, size, tmpFile);
    	if (res == -1)
			res = -errno;

		tmpFilelen = ftell(tmpFile);
		fprintf(stderr, "WRITE: Size of tmpFile after write %zu\n", tmpFilelen);

		fseek(tmpFile, 0, SEEK_SET);

		fprintf(stderr, "WRITE: Encrypting new contents of tmpFile into original file\n");

		if(!do_crypt(tmpFile, f, ENCRYPT, XMP_DATA->key_phrase))
			fprintf(stderr, "WRITE: do_crypt failed\n");

		fclose(f);
		fclose(tmpFile);
		remove(tmpPath);
    	
	}

	/* If the file to be written to is unencrypted */
	else if (crypt_action == PASS_THROUGH)
	{
		fprintf(stderr, "WRITE: File to be written is unencrypted");

		fd = open(fpath, O_WRONLY);
		if (fd == -1)
			return -errno;

		res = pwrite(fd, buf, size, offset);
		if (res == -1)
			res = -errno;

		close(fd);
   	}
   	
	free(tmpval);
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

/* Encryption happens here. No decryption */
/// PROBLEM HERE!!!!!!!!!!!!!!!!!!!!!1
static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) 
{
    (void) fi;
    (void) mode;
    char fpath[PATH_MAX];
    FILE* f = fopen(fpath, "wb+"); // Empty writable binary file.

    fprintf(stderr, "CREATE: fpath: %s\n", fpath);

	xmp_fullpath(fpath, path);

    /*
    *	Encrypt a file into itself as long as it is empty
    *	otherwise the contents of the file would be erased.
    */
	if(!do_crypt(f, f, ENCRYPT, XMP_DATA->key_phrase))
		fprintf(stderr, "CREATE: do_crypt failed\n");

	fprintf(stderr, "CREATE: encryption done correctly\n");

	fclose(f);

	if(setxattr(fpath, XATRR_ENCRYPTED_FLAG, ENCRYPTED, 4, 0))
	{
    	fprintf(stderr, "CREATE: error setting xattr of file %s\n", fpath);
    	return -errno;
   	}
   	fprintf(stderr, "CREATE: file xatrr correctly set %s\n", fpath);
    

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

	/* FOR DEBUGGING. Display what has been stored in xmp_data */
	char* mount_dir_path = realpath(argv[3], NULL);
	fprintf(stdout, "mount_dir = %s\n", mount_dir_path);
	fprintf(stdout, "mirror_dir = %s\n", (*xmp_data).mirror_dir);
	fprintf(stdout, "key_phrase = %s\n", (*xmp_data).key_phrase);
	
	/* Will pass in the mount directory and flags to fuse_main to use */
	argv[1] = argv[3]; 	// Mount directory
	argv[2] = argv[4];	// Flag (e.g. -d)
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