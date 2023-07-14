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
	size_t size;
	size_t total_size;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
	struct block *last_block;
	int last_byte;
	int rights_flags;
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

static inline char check_fd(int fd) {
	return fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL;
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
	f->size = 0;
	f->total_size = 0;
	return f;
}

static void add_file(struct file *f) {
	struct file *cur_file = file_list;
	struct file *prev_file = NULL;
	while(cur_file != NULL) {
		prev_file = cur_file;
		cur_file = cur_file->next;
	}
	if(file_list == NULL) {
		file_list = f;
		return;
	}
	prev_file->next = f;
	prev_file->next->prev = prev_file;
}

static struct filedesc *create_empty_fd(struct file *f, int flags) {
	struct filedesc *fd = malloc(sizeof(struct filedesc));
	fd->file = f;
	fd->file->refs++;
	fd->last_block = f->block_list;
	fd->last_byte = 0;
	fd->rights_flags = flags;
	return fd;
}

static int add_fd(struct file *file, int flags) {
	if(file_descriptor_count == file_descriptor_capacity) {
		file_descriptor_capacity += 100;
		file_descriptors = realloc(file_descriptors, sizeof(struct filedesc *)*file_descriptor_capacity);
		// file_descriptors[file_descriptor_count] = calloc(100, sizeof(struct filedesc *));
		for(int i = file_descriptor_capacity-100; i < file_descriptor_capacity; ++i)
			file_descriptors[i] = NULL;
	}
	for(int i = 0; i < file_descriptor_capacity; ++i) {
		if(file_descriptors[i] == NULL) {
			file_descriptors[i] = create_empty_fd(file, flags);
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
	// printf("%p\n", f_to_open);
	// if(f_to_open)
	// 	printf("%s\n", f_to_open->name);

	if(f_to_open == NULL && (flags & UFS_CREATE)) {
		f_to_open = create_empty_file(filename);
		add_file(f_to_open);
		return add_fd(f_to_open, flags);
	} else if(f_to_open == NULL || f_to_open->deleted) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	} else {
		return add_fd(f_to_open, flags);
	}
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if(check_fd(fd)) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if(file_descriptors[fd]->rights_flags & UFS_READ_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}

	struct file *f = file_descriptors[fd]->file;
	if(size + f->size > MAX_FILE_SIZE){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	// printf("write\n");

	struct block *cur_b = file_descriptors[fd]->last_block;
	int buf_shift = 0;
	while(size) {
		if(cur_b->memory == NULL){
			cur_b->memory = malloc(BLOCK_SIZE);
			f->total_size += BLOCK_SIZE;
		}

		size_t cur_sz = BLOCK_SIZE - file_descriptors[fd]->last_byte;
		if(size < cur_sz) cur_sz = size;

		// printf("SIZE: %ld, CUR_SZ: %ld\n", size, cur_sz);

		memcpy(cur_b->memory + file_descriptors[fd]->last_byte, buf+buf_shift, cur_sz);
		file_descriptors[fd]->last_byte += cur_sz;
		if(cur_b->occupied < file_descriptors[fd]->last_byte)
			cur_b->occupied = file_descriptors[fd]->last_byte;
		buf_shift += cur_sz;
		size -= cur_sz;

		if(size){
			if(cur_b->next == NULL){
				cur_b->next = create_empty_block();
		if(size + f->size > MAX_FILE_SIZE){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}		cur_b->next->prev = cur_b;
			}
			cur_b = cur_b->next;
			file_descriptors[fd]->last_block = cur_b;
			file_descriptors[fd]->last_byte = 0;
		}
	}
	f->last_block = cur_b;
	f->size += buf_shift;
	return buf_shift;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	if(check_fd(fd)) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if(file_descriptors[fd]->rights_flags & UFS_WRITE_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	
	// printf("read\n");
	// printf("NAME: %s, FILE SIZE: %ld\n", file_descriptors[fd]->file->name, file_descriptors[fd]->file->size);
	
	struct block *cur_b = file_descriptors[fd]->last_block;
	// printf("LAST FD BLOCK: %p, OCCUPIED OF FD LB: %d\n", cur_b, file_descriptors[fd]->last_byte);
	int buf_shift = 0;
	while(cur_b && size && file_descriptors[fd]->last_byte != cur_b->occupied) {
		size_t cur_sz = cur_b->occupied-file_descriptors[fd]->last_byte;
		if(size < cur_sz) cur_sz = size;

		// printf("SIZE: %ld, CUR_SZ: %ld\n", size, cur_sz);

		memcpy(buf + buf_shift, cur_b->memory+file_descriptors[fd]->last_byte, cur_sz);

		file_descriptors[fd]->last_byte += cur_sz;
		buf_shift += cur_sz;
		size -= cur_sz;
		
		if(file_descriptors[fd]->last_byte == BLOCK_SIZE){
			cur_b = cur_b->next;
			file_descriptors[fd]->last_block = cur_b;
			file_descriptors[fd]->last_byte = 0;
		}
	}

	// printf("%d\n", buf_shift);
	return buf_shift;
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

static void delete_fd(int fd) {
	if(--file_descriptors[fd]->file->refs == 0 && file_descriptors[fd]->file->deleted) {
		free(file_descriptors[fd]->file->name);
		block_list_delete(file_descriptors[fd]->file->block_list);
		free(file_descriptors[fd]->file);

		if(file_descriptors[fd]->file == file_list)
			file_list = file_list->next;
	}
	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;
}

int
ufs_close(int fd)
{
	if(check_fd(fd)) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	delete_fd(fd);
	return 0;
}

int
ufs_delete(const char *filename)
{
	struct file * f = find_file(filename);
	// printf("%p, %s\n", f, filename);
	if(f == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	

	if(f->prev)
		f->prev->next = f->next;
	if(f->next)
		f->next->prev = f->prev;
	if(f == file_list)
		file_list = file_list->next;
	if(f->refs) {
		f->deleted = 1;
		return 0;
	}

	// printf("%s - DELETED\n", filename);
	free(f->name);

	block_list_delete(f->block_list);
	free(f);

	

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

static void fix_fds(struct file *f) {
	for(int i = 0; i < file_descriptor_capacity; ++i) {
		if(file_descriptors[i] && file_descriptors[i]->file == f) {
			struct block *lb = file_descriptors[i]->last_block;

			struct block *cur_b = f->block_list;
			while(cur_b != f->last_block) {
				if(cur_b == lb) {
					break;
				}
				cur_b = cur_b->next;
			}

			if(cur_b != lb) {
				file_descriptors[i]->last_block = cur_b;
				file_descriptors[i]->last_byte = cur_b->occupied;
			} else if(lb == f->last_block) {
				if(file_descriptors[i]->last_byte > lb->occupied)
					file_descriptors[i]->last_byte = lb->occupied;
			}
		}
	}
}

int
ufs_resize(int fd, size_t new_size) {
	if(check_fd(fd)) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if(new_size > MAX_FILE_SIZE){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	struct file *f = file_descriptors[fd]->file;
	if(f->total_size < new_size) {
		struct block *cur_b = f->last_block;

		while(new_size > f->total_size) {
			if(cur_b->memory == NULL) {
				cur_b->memory = malloc(BLOCK_SIZE);
				f->total_size += BLOCK_SIZE;
			}
			if(new_size > f->total_size) {
				if(cur_b->next == NULL){
					cur_b->next = create_empty_block();
					cur_b->next->prev = cur_b;
				}
				cur_b = cur_b->next;
				f->last_block = cur_b;
			}
		}
	} else if(f->total_size > new_size) {

		if(f->total_size-BLOCK_SIZE < new_size) {
			if(f->size > new_size) {
				f->size = new_size;
				f->last_block->occupied = f->size % BLOCK_SIZE;
				fix_fds(f);
			}
		} else {
			while(f->total_size-BLOCK_SIZE > new_size) {
				struct block *tmp = f->last_block;
				f->last_block = f->last_block->prev;
				f->size -= tmp->occupied;
				f->total_size -= BLOCK_SIZE;
				block_delete(tmp);
			}
			if(f->size > new_size) {
				f->size = new_size;
				f->last_block->occupied = f->size % BLOCK_SIZE;
			}
			fix_fds(f);
		}
	}
	return 0;
}