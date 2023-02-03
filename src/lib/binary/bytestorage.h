#ifndef _BYTESTORAGE_H
#define _BYTESTORAGE_H

#include <v8.h>
#include <string>
#include <stdlib.h>

class ByteStorage;
class ByteStorageData {
public:
	//ByteStorageData(size_t _length);
	ByteStorageData(size_t _length,size_t _allocated_length=0);
	~ByteStorageData();
	size_t getInstances();
	void setInstances(size_t instances);
	char * getData();
	size_t getLength();
	size_t getAllocatedLength();
	void add(const char *add,size_t _length);
private:
	char * data;
	size_t length;
	size_t allocated_length;
	size_t instances;

	friend ByteStorage;
};

/**
 * Generic byte storage class. Every Buffer instance has this one.
 */
class ByteStorage {
public:
	ByteStorage(size_t length); /* empty */
	ByteStorage(ByteStorageData *data); /* empty */
	ByteStorage(char * data, size_t length); /* with contents (copied) */
	ByteStorage(ByteStorage * master, size_t index1, size_t index2); /* new view */
	~ByteStorage();
	
	ByteStorageData * getStorage();
	
	char * getData();
	size_t getLength();
	char getByte(size_t index);
	void setByte(size_t index, char byte);
	
	void fill(char fill);
	void fill(char * data, size_t length);
	
	ByteStorage * transcode(const char * from, const char * to);

protected:

private:
	char * data;
	size_t length;
	ByteStorageData * storage;
};

#endif
