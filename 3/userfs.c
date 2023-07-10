#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
	char deleted;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
	struct block *last_block;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

/**
 * @param filename Name of a file to find.
 * @retval NULL - Error occurred.
 *     - file * - found file.
*/
static struct file* find_file(const char* filename) {
	struct file* cur_file = file_list;
	while(cur_file != NULL) {
		if(strcmp(cur_file->name, filename) == 0) {
			return cur_file;
		}
		cur_file = cur_file->next;
	}
	return cur_file;
}

static struct block * create_empty_block() {
	struct block *b = malloc(sizeof(struct block));
	b->memory = NULL;
	b->occupied = 0;
	b->next = NULL;
	b->prev = NULL;
	return b;
}

static struct file * create_empty_file(const char *filename) {
	struct file *f = malloc(sizeof(struct file));
	//create empty block
	f->block_list = create_empty_block();
	f->last_block = f->block_list;

	f->refs = 0;
	f->name = malloc(strlen(filename)+1);
	memcpy(f->name, filename, strlen(filename)+1);
	f->next = NULL;
	f->prev = NULL;
	f->deleted = 0;
	return f;
}

static void add_file(struct file *f) {
	struct file **cur_file = &file_list;
	struct file *prev_file = NULL; 
	while(*cur_file != NULL) {
		prev_file = *cur_file;
		*cur_file = (*cur_file)->next;
	}
	*cur_file = f;
	(*cur_file)->prev = prev_file;
}

static struct filedesc *create_empty_fd(struct file *f) {
	struct filedesc *fd = malloc(sizeof(struct filedesc));
	fd->file = f;
	fd->file->refs++;
	fd->last_block = f->block_list;
	return fd;
}

static int add_fd(struct file *file) {
	if(file_descriptor_count == file_descriptor_capacity) {
		file_descriptor_capacity += 100;
		file_descriptors = realloc(file_descriptors, sizeof(struct filedesc *)*file_descriptor_capacity);
		// file_descriptors[file_descriptor_count] = calloc(100, sizeof(struct filedesc *));
		for(int i = file_descriptor_capacity-100; i < file_descriptor_capacity; ++i)
			file_descriptors[i] = NULL;
	}
	for(int i = 0; i < file_descriptor_capacity; ++i) {
		if(file_descriptors[i] == NULL) {
			file_descriptors[i] = create_empty_fd(file);
			++file_descriptor_count;
			return i;
		}
	}
	return -1;
}

int
ufs_open(const char *filename, int flags)
{
	struct file *f_to_open = find_file(filename);
	if(f_to_open == NULL && (flags & UFS_CREATE)) {
		f_to_open = create_empty_file(filename);
		add_file(f_to_open);
		return add_fd(f_to_open);
	} else if(f_to_open == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	} else {
		return add_fd(f_to_open);
	}
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if(fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct file *f = file_descriptors[fd]->file;
	
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

static void delete_fd(int fd) {
	if(--file_descriptors[fd]->file->refs == 0 && file_descriptors[fd]->file->deleted) {
		ufs_delete(file_descriptors[fd]->file->name);
	}
	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;
}

int
ufs_close(int fd)
{
	if(fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	delete_fd(fd);
	return 0;
}

static void block_delete(struct block *b) {
	free(b->memory);
	if(b->next)
		b->next->prev = b->prev;
	if(b->prev)
		b->prev->next = b->next;
	free(b);
}

static void block_list_delete(struct block *list) {
	while(list != NULL) {
		struct block *tmp = list->next;
		block_delete(list);
		list = tmp;
	}
}

int
ufs_delete(const char *filename)
{
	struct file * f = find_file(filename);
	if(f == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if(f->refs) {
		f->deleted = 1;
		return 0;
	}
	free(f->name);

	if(f->prev)
		f->prev->next = f->next;
	if(f->next)
		f->next->prev = f->prev;
	block_list_delete(f->block_list);
	free(f);

	if(f == file_list)
		file_list = NULL;

	return 0;
}

void
ufs_destroy(void)
{
	for(int i = 0; i < file_descriptor_capacity; ++i) {
		ufs_close(i);
	}
	free(file_descriptors);
	file_descriptors = NULL;

	while(file_list != NULL) {
		struct file *tmp = file_list->next;
		ufs_delete(file_list->name);
		file_list = tmp;
	}
	free(file_list);
	file_list = NULL;
}
