#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/*
#define O_TRUNC         00001000
#define O_RDWR          00000002
#define O_WRONLY        00000001
#define O_APPEND        00002000
*/

int (*real_open)(const char *pathname, int flags, ...);
int (*real_openat)(int dirfd, const char *pathname, int flags, ...);

/*** glibc functions ****/
FILE *(*real_fopen) (const char *__restrict __filename, const char *__restrict __modes)=NULL;
FILE *(*real_fopen64) (const char *__restrict __filename, const char *__restrict __modes)= NULL;
//FILE (*fdopen) (int fildes, const char *mode);
int (*real_rename)(const char *old, const char *new)=NULL;
int (*real_renameat) (int __oldfd, const char *__old, int __newfd,
                     const char *__new) = NULL;
int (*real_remove) (const char *path) =NULL;
int (*real_unlink) (const char *filename) = NULL;
int (*real_unlinkat)(int dirfd, const char *pathname, int flags) = NULL;
int (*real_rmdir) (const char *filename) = NULL;
/*** glibc functions ***/

bool is_write_flag(int flag);
bool is_write_mode (const char *mode);

void create_backup(const char *path_name);

bool is_regular_file(const char *path_name);

bool is_regular_file(const char *path_name) 
{
    struct stat path_stat;
    stat(path_name, &path_stat);
    return (S_ISREG(path_stat.st_mode) || S_ISDIR(path_stat.st_mode));
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void create_backup(const char *path_name) {
        char mypath[4096];
        char *result = concat("~/mybackup ", path_name);
        //printf("Concat %s\n", result);
        int status = system(result);
        free(result);
}


bool is_write_flag(int flag) {
	if ((flag & O_WRONLY) || (flag & O_RDWR) || (flag & O_APPEND) || (flag & O_TRUNC))
		return true;
	return false;
}

bool is_write_mode (const char *mode) {
	if (!strcmp(mode, "w") || !strcmp(mode, "wb") || !strcmp(mode, "a") ||  !strcmp(mode, "ab") || !strcmp(mode, "r+")
		|| !strcmp(mode, "rb+") || !strcmp(mode, "r+b") || !strcmp(mode, "w+") || !strcmp(mode, "wb+") 
		|| !strcmp(mode, "w+b") || !strcmp(mode, "a+") || !strcmp(mode, "ab+") || !strcmp(mode, "a+b"))
			return true;
	return false;
}

int open(const char *file, int flags, ...) 
{

	mode_t mode;
	va_list argp;
	va_start(argp, flags);
 	int fd;
	//perror("Error in open: ");
	//printf("Raw open detected with flags\n");
	//real_open(pathname, flags);
	if(is_write_flag(flags)) {
		if (is_regular_file(file)) {
			create_backup(file);
			//printf("Open detected open with flags\n");
		}
	}

	real_open = dlsym(RTLD_NEXT, "open");
	if (flags & O_CREAT) {
		mode = va_arg(argp, mode_t);
		real_open(file, flags, mode);
  	}
	else {
		real_open(file, flags);
	}
}


int open64(const char *file, int flags, ...) 
{

	mode_t mode;
	va_list argp;
	va_start(argp, flags);
 	int fd;
	real_open = dlsym(RTLD_NEXT, "open");
	//perror("Error in open: ");
	//printf("Raw open detected with flags\n");
	//real_open(pathname, flags);
	if(is_write_flag(flags)) {
		if (is_regular_file(file)) {
			create_backup(file);
			//printf("Open detected open with flags\n");
		}
	}

	if (flags & O_CREAT) {
		mode = va_arg(argp, mode_t);
		real_open(file, flags, mode);
  	}
	else {
		real_open(file, flags);
	}
}

int openat(int dirfd, const char *pathname, int flags, ...)
{

	mode_t mode;
	va_list argp;
	va_start(argp, flags);
 	int fd;
	//perror("Error in openat: ");
	//printf("Raw openat detected with flags\n");
	real_openat = dlsym(RTLD_NEXT, "openat");
	//real_open(pathname, flags);
	if(is_write_flag(flags)) {
		if (is_regular_file(pathname)) {
			create_backup(pathname);
			//printf("Open detected open with flags\n");
		}
	}

	if (flags & O_CREAT) {
		mode = va_arg(argp, mode_t);
		real_openat(dirfd, pathname, flags, mode);
  	}
	else {
		real_openat(dirfd, pathname, flags);
	}
}


FILE *fopen (const char *filename, const char *mode) {
	if (is_write_mode(mode)) {

		if (is_regular_file(filename)) {
			create_backup(filename);
			//printf("Open detected open with flags\n");
		}
	}
	real_fopen = dlsym(RTLD_NEXT, "fopen");
	real_fopen(filename, mode);
}

FILE *fopen64 (const char *__restrict __filename, const char *__restrict __modes) {

	if (is_write_mode(__modes)) {
		if (is_regular_file(__filename)) {
			create_backup(__filename);
		}
	}
	real_fopen64 = dlsym(RTLD_NEXT, "fopen64");
	real_fopen64(__filename, __modes);
}

int rename (const char *old, const char *new) {

	if (is_regular_file(old)) {
		create_backup(old);
	}
	real_rename = dlsym(RTLD_NEXT, "rename");
	real_rename(old, new);
}

int renameat (int __oldfd, const char *__old, int __newfd, const char *__new) {

	if (is_regular_file(__old)) {
		create_backup(__old);
	}
	real_renameat = dlsym(RTLD_NEXT, "renameat");
	real_renameat(__oldfd, __old, __newfd, __new);

}

int remove (const char *path) {

	if (is_regular_file(path)) {
		create_backup(path);
	}
	real_remove = dlsym(RTLD_NEXT, "remove");
	real_remove(path);
}

int unlink (const char *filename) {

	if (is_regular_file(filename)) {
		create_backup(filename);
	}
	real_unlink = dlsym(RTLD_NEXT, "unlink");
	real_unlink(filename);
}

int unlinkat (int dirfd, const char *pathname, int flags) {

	if (is_regular_file(pathname)) {
		create_backup(pathname);
	}
	real_unlinkat = dlsym(RTLD_NEXT, "unlinkat");
	real_unlinkat(dirfd, pathname, flags);
}

int rmdir (const char *filename) {

	if (is_regular_file(filename)) {
		create_backup(filename);
	}
	real_rmdir  = dlsym(RTLD_NEXT, "rmdir");
	real_rmdir(filename);
}

