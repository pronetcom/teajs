#include "archiverBase.h"
#include <string>
#include <zip.h>
#include <vector>

class ArchiverZip : public virtual ArchiverBase {
public:
    ArchiverZip();
    ~ArchiverZip();
    const std::string getError() const;
    bool open(std::string path, int flags);
    bool close();
    int64_t addFile(std::string fileNameInZip, const char* source, size_t sourceSize, bool useAsBuffer = false);
    int64_t addDir(std::string dirNameInZip);
    ByteStorage* readFileByName(const std::string& fileNameInZip, const uint64_t maxSize);
    ByteStorage* readFileByIndex(const int64_t fileIndexInZip, const uint64_t maxSize);
    char* getArchive(size_t *len);
private:
    zip_t* archive;
    std::string jsError;
    std::vector<void*> buffers;
};
