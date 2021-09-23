/*
 * Copyright (c) 2019, Xdevelnet (xdevelnet at xdevelnet dot org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>
#include <iso646.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <sys/resource.h>

#define strizeof(a) (sizeof(a)-1)
#define SSB_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100) // MUHAHAHAH, I REALLY FOUND THIS MAGNIFICENT <3 <3 <3
#define IS_LITTLE_ENDIAN (((union { unsigned x; unsigned char c; }){1}).c) // requires c99
#define POSIX_FAIL -1
#define STACK_REDUNDANCE 1024

const char essb_signature[] = "SSBTEMPLATE0";
const char ssb_ext_w_dot[] = ".ssb";
const char html_ext_w_dot[] = ".html";
const char opening_scope_sigil[] = "{{";
const char closing_scope_sigil[] = "}}";
const char parser_error_string[] = "Invalid template format. Check filesize or tag names sizes.";
const char error_occured_formatted[] = "An error occured during processing %s. Error reason: %s\n";
const char template_processing_done_formatted[] = "Template processing is done, result has been saved to %s%s\n";
const char error_stack_reached[] = "Filepath is bigger then available stack! U MAD BRUH!";
const uint32_t max_tag_length = 2147483646; // 2^32/2-2

int32_t *ve;
size_t v_max = 5;
size_t v_amount;

static void vec_reset() {
	v_amount = 0;
}

static bool vec_add(int32_t val) {
	ve[v_amount] = val;
	v_amount++;
	if (v_amount == v_max) {
		v_max += 10;
		int32_t *temp = realloc(ve, v_max * sizeof(int32_t));
		if (temp == NULL) return false;
		ve = temp;
	}

	return true;
}

static inline bool correct_html_ext(const char *p) {
	// above
	// Check if an extension in filename, referenced by pointer p, is correct.

	size_t len = strlen(p);
	if (
		len <= strizeof(html_ext_w_dot)
		or
		memcmp(p + len - strizeof(html_ext_w_dot), html_ext_w_dot, strizeof(html_ext_w_dot)) != 0
	) return false;
	return true;
}

int fstat_getsize(int fd, size_t *size) {
	// above
	// Retrieve size of file from _fd_ file descriptor and save it to _size_ addr.
	// Return value is same as you gonna use fstat()

	struct stat st;
	int rval = fstat(fd, &st);
	if (rval < 0) return rval;
	*size = st.st_size;
	return rval;
}

static inline ssize_t nposix_pwrite(int fd, void *buf, size_t count, off_t offset) {
	// above
	// it's pread() when available
	// If system haven't required standard, then use non-atomic usage of lseek() and read().

	const signed posix_fail_val = -1;
	#if (_XOPEN_SOURCE) >= 500 || (_POSIX_C_SOURCE) >= 200809L
		return pwrite(fd, buf, count, offset);
	#else
		off_t backup = lseek(fd, 0, SEEK_CUR);
		if (backup < 0 or lseek(fd, offset, SEEK_SET) < 0) return posix_fail_val;
		ssize_t rval = write(fd, buf, count); // for various reasons there is no reason to check return values of these calls
		lseek(fd, backup, SEEK_SET); // e.g. do we need to restore if write() will fail? Ughh... yes? At least an attempt?
		return rval;
	#endif
}

static inline void *dumbmap(int fd, size_t size, int prot) {
	// above
	// Like mmap, but simplified. Useful if you want to map whole file and if you're an idiot like me.

	return mmap(NULL, size, prot, MAP_SHARED, fd, 0);
}

char *getrptr_if_correct_template(const char *fname, size_t *sptr) {
	// above
	// Check if template with name fname is correct and save filesize to variable, referenced by sptr pointer

	int fd = POSIX_FAIL;
	if (
		access(fname, R_OK) < 0
		or
		(fd = open(fname, O_RDONLY)) < 0
		or
		fstat_getsize(fd, sptr) < 0
	) return perror(fname), close(fd), NULL;
	if (*sptr == 0) return close(fd), NULL;
	char *retval = dumbmap(fd, *sptr, PROT_READ);
	if (retval == MAP_FAILED) return close(fd), NULL;
	close(fd);
	return retval;
}

int getwfd(const char *rfname, const char *ext, size_t elen, char *wrfname_done) {
	// above
	// Create file with name, which is referenced by rfname pointer, but with different extension, which is
	// referenced by ext pointer. Both pointers MUST be null terminated strings.
	// Also stores produced filename to wrfname_done.

	char *dotpos = strrchr(rfname, '.');
	if (dotpos == NULL) return POSIX_FAIL;
	size_t rlen = dotpos - rfname;
	memcpy(wrfname_done, rfname, rlen);
	memcpy(wrfname_done + rlen, ext, elen);
	wrfname_done[rlen + elen] = '\0';
	int fd = open(wrfname_done, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0) return perror(wrfname_done), POSIX_FAIL;
	return fd;
}

#define WFDRITE(b, c) if (write((wfd),(b),(c))<0)goto posix_error // return if write() fails
#define ADDRITE(c) if (vec_add(c) == false)goto posix_error

#define DSLICE_ADD(b, c) {            \
	jump_to_table += (c);             \
	ADDRITE(c);                       \
	WFDRITE(b, c);                    \
	}

#define DSLICE_ADD_TAG(b, c) {        \
	jump_to_table += (c);             \
	ADDRITE(-(c));                    \
	WFDRITE(b, c);                    \
	}

void parse(char *data, size_t size, int wfd, const char **error) {
	// above
	// Evaluate parsing, write results to ssb file, referenced by wfd file descriptor.
	// If parsing fails, set error pointer to null-terminated string, which describes error reason.

	errno = 0;
	uint32_t jump_to_table = 0;
	uint32_t count = 0;
	WFDRITE(essb_signature, strizeof(essb_signature));
	WFDRITE(&jump_to_table, sizeof(jump_to_table));
	WFDRITE(&count, sizeof(count));
	vec_reset();
	size_t current_position = 0;
	while(current_position < size) {
		char *found = strstr(data + current_position, opening_scope_sigil);
		if (found == NULL) {
			DSLICE_ADD(data + current_position, size - current_position);
			count++;
			break;
		}
		if (found != data + current_position) { // don't need to add data if we found another tag right after tag
			count++;
			DSLICE_ADD(data + current_position, found - data - current_position);
		}
		current_position = found - data + strizeof(opening_scope_sigil);
		found = strstr(data + current_position, closing_scope_sigil);
		if (found == NULL) continue; else {
			count++;
			if (found - data - current_position > max_tag_length) goto parse_error;
			DSLICE_ADD_TAG(data + current_position, found - data - current_position);
			current_position = found - data + strizeof(closing_scope_sigil);
		}
	}
	static const char empty[3];
	unsigned residue = 4 - (jump_to_table % 4);
	if (residue != 4) WFDRITE(empty, residue);
	WFDRITE(ve, v_amount * sizeof(int32_t));
	if (nposix_pwrite(wfd, &count, sizeof(count), strizeof(essb_signature)) < 0) goto posix_error;
	if (nposix_pwrite(wfd, &jump_to_table, sizeof(jump_to_table), strizeof(essb_signature) + sizeof(count)) < 0) goto posix_error;
	*error = NULL;
	return;
	posix_error:
	*error = strerror(errno);
	return;
	parse_error:
	*error = parser_error_string;
}

int main(int argc, char **argv) {
	char *dpath;
	if (argc < 2) dpath = "./"; else dpath = argv[1];
	DIR *d = opendir(dpath);
	if (d == NULL or chdir(dpath) < 0) {
		perror(dpath);
		return EXIT_FAILURE;
	}
	struct rlimit l;
	if (getrlimit(RLIMIT_STACK, &l) < 0) return perror("GETRLIMIT FAILED!"), EXIT_FAILURE;
	size_t usable_stack_amount = l.rlim_cur - STACK_REDUNDANCE;
	ve = malloc(v_max * sizeof(int32_t));
	if (ve == NULL) return perror("failed to allocate such small amount of memory, lool"), EXIT_FAILURE;

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (correct_html_ext(ent->d_name) == false) continue;
		size_t size;
		char *data = getrptr_if_correct_template(ent->d_name, &size);
		if (data == NULL) break;
		if (strlen(ent->d_name) + strizeof(ssb_ext_w_dot) > usable_stack_amount) {
			munmap(data, size);
			puts(error_stack_reached);
			break;
		}
		char produced_filename[strlen(ent->d_name) + strizeof(ssb_ext_w_dot)];
		int wfd = getwfd(ent->d_name, ssb_ext_w_dot, strizeof(ssb_ext_w_dot), produced_filename);
		if (wfd < 0) {munmap(data, size); break;}
		const char *errstr;
		parse(data, size, wfd, &errstr);
		close(wfd);
		if (errstr != NULL) {
			fprintf(stderr, error_occured_formatted, ent->d_name, errstr);
		} else {
			char *pathpr = "";
			if (dpath == argv[1]) pathpr = dpath;
			fprintf(stderr, template_processing_done_formatted, pathpr, produced_filename);
		}
		munmap(data, size);
	}
	free(ve);
	closedir(d);
	return EXIT_SUCCESS;
}
