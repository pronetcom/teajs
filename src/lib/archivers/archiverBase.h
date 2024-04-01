#ifndef HEADERFILE_H
#define HEADERFILE_H


#include "macros.h"

class ArchiverBase {
public:
    virtual const std::string getError() const;
    virtual bool open(std::string path, int flags);
    virtual bool close();
    virtual int64_t addFile(std::string fileNameInZip, const char* source, size_t sourceSize, bool useAsBuffer = false);
    virtual int64_t addDir(std::string dirNameInZip);
    virtual int64_t readFileByName(const std::string& fileNameInZip, const uint64_t maxSize, ByteStorage* result);
    virtual int64_t readFileByIndex(const int64_t fileIndexInZip, const uint64_t maxSize, ByteStorage* result);
    // virtual void* getArchive();
    virtual ~ArchiverBase();
    enum Type {
        ZIP = 0
    };
protected:
};
#endif