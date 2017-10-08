/*
 * CachingFileSystem.cpp
 */

#define FUSE_USE_VERSION 26
#define FILE_PATH_TOO_LONG -1
#define FAILURE -1

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <math.h>
#include <sys/stat.h>
#include <deque>
#include <string.h>
//#include <bits/basic_string.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <bits/stringfwd.h>
#include <iosfwd>
#include <stdlib.h>
#include <algorithm>
#include <fstream>
#include <cstring>


using namespace std;

struct fuse_operations caching_oper;
struct caching_state {
    char *rootdir;
};



/* GLOBAL VARIABLES */
static int fNew;
static int fMiddle;
static int fOld;
static char* mountdir;
static char* rootdir;
static size_t blockSize;
static size_t blockNumber;
static fstream log_file;


typedef struct block
{
    char* filePath;
    char* data;
    int numOfAccess;
    int begining;
    int fd;
}block;


static deque<block*> cache;


#define CACHING_DATA ((struct caching_state *) fuse_get_context()->private_data)
#define ARG_EXPECTED 5


/**
 * writes to the log file
 */
void log_msg(time_t time, string funcName)
{
    log_file << (long)time << "  " << funcName << endl;
}


/**
 * creates the full path of a given path.
 */
void getFullPath(char* fpath[PATH_MAX], const char *path)
{
    strcpy(*fpath, CACHING_DATA->rootdir);
    strncat(*fpath, path, PATH_MAX);
}


/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int caching_getattr(const char *path, struct stat *statbuf)
{
    if (strcmp(path,"/.filesystem.log") == 0)
    {
        return -ENOENT;
    }

    log_msg(time(NULL), "getattr");
    int retstat;
    char* fpath = new char[PATH_MAX];
    getFullPath(&fpath,path);
    retstat = lstat(fpath,statbuf);
    if (retstat < 0)
    {
        return (-errno);
    }
    return retstat;
    delete(fpath);
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int caching_fgetattr(const char *path, struct stat *statbuf,
                     struct fuse_file_info *fi)
{
    if (strcmp(path,"/.filesystem.log") == 0)
    {
        return -ENOENT;
    }
    log_msg(time(NULL), "fgetattr");
    int retstat = 0;
    retstat = fstat((int)fi->fh,statbuf);
    if (retstat < 0)
    {
        return (-errno);
    }
    return retstat;
}


/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int caching_access(const char *path, int mask)
{
    log_msg(time(NULL), "access");

    if (strcmp(path,"/.filesystem.log") == 0)
    {
        return -ENOENT;
    }
    int retstat = 0;
    char* fpath = new char[PATH_MAX];

    getFullPath(&fpath, path);
    retstat = access(fpath, mask);

    if (retstat < 0)
    {
        return -errno;
    }

    delete(fpath);
    return retstat;
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)size
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * initialize an arbitrary filehandle (fh) in the fuse_file_info
 * structure, which will be passed to all file operations.

 * pay attention that the max allowed path is PATH_MAX (in limits.h).
 * if the path is longer, return error.

 * Changed in version 2.2
 */
int caching_open(const char *path, struct fuse_file_info *fi){

    log_msg(time(NULL), "open");

    fi->direct_io = 1;
    int retstat = 0;
    int fd;
    char* fpath = new char[PATH_MAX];
    getFullPath(&fpath, path);


    //TODO remember to put this in every function beside dir-functions.
    // #2nd solution
     if (strcmp(path,"/.filesystem.log") == 0)
     {
       return -ENOENT;
     }


    if (fi->flags & (O_CREAT | O_EXCL | O_TRUNC))
    {
        // error of permission denied for the file
        return -EACCES;
    }

    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1
    fd = open(fpath, O_RDONLY|O_DIRECT|O_SYNC);

    if(fd < 0)
    {
        return -errno;
    }

    fi->fh = fd;
    //delete(fpath);
    return retstat;
}

/**
 * removes a specific block from the cach.
 * the removed block belongs to the "old" section.
 */
void removeFromCache(const char* path)
{
    unsigned long oldIdx = (unsigned long)fNew + fMiddle;
    unsigned long indexOfLFU = oldIdx;
    int minAccess = cache.at(oldIdx)->numOfAccess;;
    for (;oldIdx < cache.size();oldIdx++)
    {
        block* tempBlock = cache.at(oldIdx);
        if (tempBlock->numOfAccess < minAccess)
        {
            minAccess = tempBlock->numOfAccess;
            indexOfLFU = oldIdx;
        }
    }
    cache.erase(cache.begin() + indexOfLFU);
}

/**
 * updates the cach
 */
int updateCache(const char *path, size_t size,
                int begining, struct fuse_file_info *fi)
{
    try {
        if (cache.size() == blockNumber)
        {
            removeFromCache(path);
        }
        void* buf = aligned_alloc(blockSize,blockSize);
        ssize_t sRead = pread((int)fi->fh,buf,blockSize,begining);
        block* b = new block();
        b->filePath = new char[PATH_MAX];
        b->data = new char[blockSize];
        memcpy(b->filePath,path,strlen(path)+1);
        memcpy(b->data,buf,blockSize);
        b->numOfAccess = 1;
        b-> begining = (int) begining;
        b->fd = (int)fi->fh;
        cache.push_front(b);
        free(buf);
        return (int)sRead;
    }
    catch (std::bad_alloc& exc)
    {
        return -1;
    }

}

/**
 * checks whether a given block is in the cach if
 * it in the cach returns its index, else reyurns -1.
 */
int isBlockInCache(int begining, const char* path)
{
    int idx = 0;
    deque<block*>::iterator it = cache.begin();
    for(; it != cache.end(); it++)
    {
        block* block = *it;
        if (strcmp(block->filePath, path) == 0 && block->begining == begining)
        {
            return idx;
        }
        idx++;
    }
    idx = -1;
    return idx;
}

/**
 * copies a specific block to the buffer
 */
void copyToBuffer(char* pString, int idx,off_t offset, int bufIdx,size_t size)
{
    memcpy(&(pString[bufIdx]),&(cache.at(idx)->data)[offset],size);
    if (idx >= fNew)
    {
        cache.at(idx)->numOfAccess++;
    }
}


/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error. For example, if you receive size=100, offest=0,
 * but the size of the file is 10, you will init only the first
   ten bytes in the buff and return the number 10.

   In order to read a file from the disk,
   we strongly advise you to use "pread" rather than "read".
   Pay attention, in pread the offset is valid as long it is
   a multipication of the block size.
   More specifically, pread returns 0 for negative offset
   and an offset after the end of the file
   (as long as the the rest of the requirements are fulfiiled).
   You are suppose to preserve this behavior also in your implementation.

 */// Changed in version 2.2sizeCopied

int caching_read(const char *path, char *buf, size_t size,
                 off_t offset, struct fuse_file_info *fi)
{
    log_msg(time(NULL), "read");
    int sizeCopied = 0;
    char* fpath = new char[PATH_MAX];
    getFullPath(&fpath,path);
    struct stat sb;
    lstat(fpath,&sb);
    int fileSize = (int) sb.st_size;

    if (offset < 0 || offset >= fileSize)
    {
        return 0;
    }
    else
    {
        //make the offset multiplaction of blockSize
        int begining = (int) (offset/blockSize)*blockSize;
        int counter = 0 ;
        int idx;
        int location_in_buf = 0;
        size_t sizeLeft = min((int)(fileSize-offset),(int)size);
        offset = offset % blockSize;
        while (sizeLeft != 0)
        {
            if (isBlockInCache(begining,fpath) < 0)
            {
                int errCheck = updateCache(fpath, size, begining, fi);
                if (errCheck == -1)
                {
                    return -errno;
                }
            }
            idx = isBlockInCache(begining,fpath);
            if (sizeLeft >= blockSize)
            {
                copyToBuffer(buf,idx,offset,location_in_buf,blockSize);
                location_in_buf += (blockSize-offset);
                sizeCopied += (blockSize -offset);
                sizeLeft -= blockSize;
            }
            else
            {
                copyToBuffer(buf,idx,offset,location_in_buf,sizeLeft);
                sizeCopied += (sizeLeft);
                sizeLeft = 0;
            }
            offset = 0;
            begining += blockSize;
            counter++;
        }
    }
    delete(fpath);
    return sizeCopied;
}


/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int caching_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg(time(NULL), "flush");
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int caching_release(const char *path, struct fuse_file_info *fi)
{
    log_msg(time(NULL), "release");
    int retval = close(fi->fh);

    if (retval < 0)
    {
        return -errno;
    }

    return retval;

}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int caching_opendir(const char *path, struct fuse_file_info *fi)
{
    cout << "inside opendir" << endl;
    DIR *dp;
    int retstat = 0;
    char *fpath = new char[PATH_MAX];
    getFullPath(&fpath, path);
    // since opendir returns a pointer, takes some custom handling of
    // return status.
    dp = opendir(fpath);
    if (dp == NULL) {
        return -errno;
    }

    fi->fh = (uint64_t) (intptr_t) dp;
    delete (fpath);
    return retstat;

}
/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * Introduced in version 2.3
 */
int caching_readdir(const char *path, void *buf,
                    fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi)
{
    log_msg(time(NULL), "readdir");

    int retstat = 0;
    DIR *dp;
    struct dirent *de;

    dp = (DIR *) (uintptr_t) fi->fh;
    de = readdir(dp);
    if (de == NULL) {
        return -errno;
    }
    // The purpose of filler function is to insert directory entries into
    // the directory structure , which is passed to our callback buf.
    do {
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
    } while ((de = readdir(dp)) != NULL);


    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int caching_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_msg(time(NULL), "releasedir");

    int retstat = 0;
    retstat = closedir((DIR *) (uintptr_t) fi->fh);

    if (retstat < 0) {
        return -errno;
    }
    return retstat;
}


/** Rename a file */
int caching_rename(const char *path, const char *newpath)
{
    log_msg(time(NULL), "rename");

    char *fpath = new char[PATH_MAX];
    getFullPath(&fpath, path);
    char *fpathNew = new char[PATH_MAX];
    getFullPath(&fpathNew, newpath);

    int retstat = rename(fpath, fpathNew);
    if (retstat < 0) {
        return (-errno);
    }
    // if the retstat was alright, we changed the file name
    // and we have to the same for those parts in the cache. (if they exist)
    deque<block *>::iterator it = cache.begin();
    for (; it != cache.end(); it++) {
        block *block = *it;
        if (strcmp(block->filePath, fpath) == 0) {
            strcpy(block->filePath, fpathNew);
        }
    }
    delete (fpath);
    delete (fpathNew);
    return 0;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *

If a failure occurs in this function, do nothing (absorb the failure
and don't report it).
For your task, the function needs to return NULL always
(if you do something else, be sure to use the fuse_context correctly).
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *caching_init(struct fuse_conn_info *conn)
{
    log_msg(time(NULL), "init");

    return CACHING_DATA;
}


/**
 * Clean up filesystem
 *
 * Called on filesystem exit.


If a failure occurs in this function, do nothing
(absorb the failure and don't report it).

 * Introduced in version 2.3
 */
void caching_destroy(void *userdata)
{
    log_msg(time(NULL), "destroy");

    deque<block *>::iterator it = cache.begin();
    for (; it != cache.end(); it++) {
        block *block = *it;
        delete (block->filePath);
        delete (block->data);
        delete (block);
    }
    // closing the log file
    log_file.close();

}


/**
 * Ioctl from the FUSE sepc:
 * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
 * 64bit environment.  The size and direction of data is
 * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
 * data will be NULL, for _IOC_WRITE data is out area, for
 * _IOC_READ in area and if both are set in/out area.  In all
 * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
 *
 * However, in our case, this function only needs to print
 cache table to the log file .
 *
 * Introduced in version 2.8
 */
int caching_ioctl(const char *path, int cmd, void *arg,
                  struct fuse_file_info *, unsigned int flags, void *data)
{
    log_msg(time(NULL), "ioctl");

    deque<block*>::iterator it = cache.begin();
    for(; it != cache.end(); it++)
    {
        block* block = *it;
        int rootDirL = strlen(rootdir);
        log_file << block->filePath + rootDirL + 1 << " " << (block -> begining) / blockSize << " " << block -> numOfAccess << endl;
    }

    log_file.flush();
    return 0;
}


// Initialise the operations.
// You are not supposed to change this function.
void init_caching_oper() {
    caching_oper.getattr = caching_getattr;
    caching_oper.access = caching_access;
    caching_oper.open = caching_open;
    caching_oper.read = caching_read;
    caching_oper.flush = caching_flush;
    caching_oper.release = caching_release;
    caching_oper.opendir = caching_opendir;
    caching_oper.readdir = caching_readdir;
    caching_oper.releasedir = caching_releasedir;
    caching_oper.rename = caching_rename;
    caching_oper.init = caching_init;
    caching_oper.destroy = caching_destroy;
    caching_oper.ioctl = caching_ioctl;
    caching_oper.fgetattr = caching_fgetattr;


    caching_oper.readlink = NULL;
    caching_oper.getdir = NULL;
    caching_oper.mknod = NULL;
    caching_oper.mkdir = NULL;
    caching_oper.unlink = NULL;
    caching_oper.rmdir = NULL;
    caching_oper.symlink = NULL;
    caching_oper.link = NULL;
    caching_oper.chmod = NULL;
    caching_oper.chown = NULL;
    caching_oper.truncate = NULL;
    caching_oper.utime = NULL;
    caching_oper.write = NULL;
    caching_oper.statfs = NULL;
    caching_oper.fsync = NULL;
    caching_oper.setxattr = NULL;
    caching_oper.getxattr = NULL;
    caching_oper.listxattr = NULL;
    caching_oper.removexattr = NULL;
    caching_oper.fsyncdir = NULL;
    caching_oper.create = NULL;
    caching_oper.ftruncate = NULL;
}



/*This function is here to make sure our division to blocks is legal
* there are some inputs that can cause troublsom situations:
* e.g blockSize = 5, oldPrec = 0.5 , newPrec = 0.5 both fNew
* and fOld will be 3, and total block size will be 6. which is a problem..
* e.g2 block size = 7 , oldPrec =0.33, newPrec =0.333. now, all three
* will be 2.33 and rounded to 2. which will cause totalBlockSize to be 6.....................;p[[]]
*/
void init_block_division(double oldPrec, double newPrec) {
    fNew = (size_t) round(newPrec * blockNumber);
    fOld = (size_t) round(oldPrec * blockNumber);
    fMiddle = (size_t) round((1 - newPrec - oldPrec) * blockNumber);
    if ((size_t)(fMiddle + fOld + fNew) < blockNumber)
    {
        fMiddle++;
    }
    else if ((size_t)(fMiddle + fOld + fNew) > blockNumber) {
        if (fMiddle >= 1) {
            fMiddle--;
        }
        else if (fNew >= 1) {
            fNew--;
        }
        else if (fOld >= 1) {
            fOld--;
        }
    }
}

//basic main. You need to complete it.
int main(int argc, char *argv[]) {

    cout << "inside main" << endl;
    bool goodParameters = true;
    struct stat fi, sb, sb2;
    stat("/tmp", &fi);
    struct caching_state *caching_data;

    //checks if number of argument is legal. if it is,
    //it initalize the global variables and check for correctness.
    if (argc - 1 != ARG_EXPECTED) {
        goodParameters = false;
    }
    else {
        rootdir = argv[1];
        mountdir = argv[2];
        blockNumber = atoi(argv[3]);
        double oldPrec = atof(argv[4]);
        double newPrec = atof(argv[5]);
        init_block_division(oldPrec, newPrec);
        blockSize = (size_t) fi.st_blksize;

        if (stat(rootdir, &sb) < 0) {
            goodParameters = false;
        }
        if (stat(mountdir, &sb2) < 0) {
            goodParameters = false;
        }
        // check if paths given are directories.
        if (!(sb.st_mode & S_IFDIR) || !(sb2.st_mode & S_IFDIR)) {
            goodParameters = false;
        }
            //checks if the blockNumber is strictly positive (legal).
        else if (blockNumber <= 0) {
            goodParameters = false;
        }
            //makes sure fNew and fOld input are legal.
        else if (oldPrec < 0 || newPrec < 0 || oldPrec + newPrec > 1) {
            goodParameters = false;
        }
    }
    if (!goodParameters) {
        cout << "Usage: CachingFileSystem rootdir mountdir numberOfBlocks fOld fNew" << endl;
        exit(0);
    }
    //creates the log file it it already open just append to it
    // creates the log file if it is already open dont open a new file.

    char* path = new char[PATH_MAX];
    strcpy(path,rootdir);
    string logpath = strcat(path, "/.filesystem.log");
    log_file.open(logpath.c_str() , std::ifstream::app);
    delete(path);


    init_caching_oper();

    caching_data = (caching_state *) malloc(sizeof(struct caching_state));
    caching_data->rootdir = realpath(rootdir, NULL);
    argv[1] = argv[2];
    for (int i = 2; i < (argc - 1); i++) {
        argv[i] = NULL;
    }
    argv[2] = (char *) "-s";
    argc = 3;
    int fuse_stat = fuse_main(argc, argv, &caching_oper, caching_data);
    return fuse_stat;
}
