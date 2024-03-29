#ifdef HAVE_MMAN_H
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <cstring>
#  include <unistd.h>
#  include <fcntl.h>
#else
#  include <cstdio>
#endif
#include <cstdlib>

#include "common.h"

const char*config_path()
{
	const char *ret=getenv("TEAJS_CONF_PATH");
	if (!ret) ret="/etc/teajs.conf";
	return ret;
}

const char*blob_path()
{
	const char *ret=getenv("TEAJS_BLOB_PATH");
	if (!ret) ret="/usr/local/lib/teajs/";
	return ret;
}

void * mmap_read(char * name, size_t * size) {
#ifdef HAVE_MMAN_H
	int f = open(name, O_RDONLY);
	if (f == -1) { return NULL; }
	*size = lseek(f, 0, SEEK_END);
	void * data = mmap(0, *size, PROT_READ, MAP_SHARED, f, 0);
	close(f);
#else
	FILE * f = fopen(name, "rb");
	if (f == NULL) { return NULL; }
	fseek(f, 0, SEEK_END);
	size_t s = ftell(f);
	*size = s;
	
	rewind(f);
	char * data = new char[s];
	for (unsigned int i=0; i<s;) {
		size_t read = fread(& data[i], 1, s-i, f);
		i += read;
	}
	fclose(f);
#endif
	return data;
}

void mmap_free(char * data, size_t size) {
#ifdef HAVE_MMAN_H
	munmap(data, size);
#else
	delete[] data;
#endif
}

int mmap_write(char * name, void * data, size_t size) {
#ifdef HAVE_MMAN_H
	int f = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (f == -1) { return -1; }
	lseek(f, size - 1, SEEK_SET);
	size_t written = write(f, "", 1);
	if (written != 1) { return -1; }
		
	void * dst = mmap(0, size, PROT_WRITE, MAP_SHARED, f, 0);
	memcpy(dst, data, size);
	munmap(dst, size);
	close(f);

#else
	FILE * f = fopen(name, "wb");
	if (f == NULL) { return -1; }
	fwrite(data, size, 1,f);
	fclose(f);
#endif
	return 0;
}
