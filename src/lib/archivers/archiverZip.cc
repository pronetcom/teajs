#include <v8.h>
#include "macros.h"
#include <cstring>

#include "archiverZip.h"

ArchiverZip::ArchiverZip() {
    archive = nullptr;
    jsError = "";
}

ArchiverZip::~ArchiverZip() {
    if(this->close()) {
        JS_ERROR(jsError);
    }
    // std::cerr << "archive close point 7" << std::endl;
}

const std::string ArchiverZip::getError() const {
    return jsError;
}

bool ArchiverZip::open(std::string path, int flags) {
    int err;
    if ((archive = zip_open(path.c_str(), flags, &err)) == nullptr) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        jsError = "Cannot open zip archive ";
        jsError += path;
        jsError += ": ";
        jsError += zip_error_strerror(&error);
        zip_error_fini(&error);
        // throw jsError; // should be catched in JS_METHOD to produce JS_ERROR
        // JS_ERROR(jsError);
        return false;
    }
    return true;
}

bool ArchiverZip::close() { // true if an error occured
    if (archive != nullptr) {
        if (zip_close(archive) < 0) {
            jsError = "Cannot close zip archive: ";
            jsError += zip_strerror(archive);
            return true;
            // std::cerr << "zip_close failed with an error: " << zip_strerror(archive) << std::endl;
        }
        archive = nullptr;
    }
    for (size_t i = 0; i < buffers.size(); i++) {
        free(buffers[i]);
    }
    buffers.clear();
    return false;
}

int64_t ArchiverZip::addFile(std::string fileNameInZip, const char* source, size_t sourceSize, bool useAsBuffer) {
    // std::cerr << "C addFile begin" << std::endl;
    // std::cerr << "fileNameInZip: " << fileNameInZip << "; source: " << source << "; useAsBuffer: " << useAsBuffer << std::endl;
    {
        void* newBuffer = malloc(sourceSize);
        memcpy(newBuffer, source, sourceSize);
        buffers.push_back(newBuffer);
    }
    zip_source_t* s = (useAsBuffer ?
        zip_source_buffer(archive, buffers.back(), sourceSize, 0) :
        zip_source_file(archive, (const char*)buffers.back(), 0, 0));
   // std::cerr << "C addFile created source" << std::endl;
    if (s == nullptr) {
        // std::cerr << "C addFile created source fail" << std::endl;
        // throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(zipper)));
        jsError = "Failed to add file to zip: ";
        jsError += zip_strerror(archive);
        return -1;
    }
    zip_int64_t result;
    if ((result = zip_file_add(archive, fileNameInZip.c_str(), s, ZIP_FL_ENC_UTF_8)) < 0) {
        // std::cerr << "C addFile add file fail" << std::endl;
        zip_source_free(s);
        // throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(archive)));
        jsError = "Failed to add file to zip: ";
        jsError += zip_strerror(archive);
        return -1;
    }
    // std::cerr << "C addFile added file" << std::endl;
    return (int64_t)result;
}

int64_t ArchiverZip::addDir(std::string dirNameInZip) {
    zip_int64_t result;
    if ((result = zip_dir_add(archive, dirNameInZip.c_str(), ZIP_FL_ENC_UTF_8)) < 0) {
        // throw std::runtime_error("Failed to add dir to zip: " + std::string(zip_strerror(archive)));
        jsError = "Failed to add file to zip: ";
        jsError += zip_strerror(archive);
        return -1;
    }
    return (int64_t)result;
}

ByteStorage* ArchiverZip::readFileByName(const std::string& fileNameInZip, const uint64_t maxSize) {
    zip_file* file;
    if (!(file = zip_fopen(archive, fileNameInZip.c_str(), 0))) {
        // fprintf(stderr, "boese, boese\n");
        // std::cout << "failed to open entry of archive." << zip_strerror(archive) << std::endl;
        // zip_close(archive);
        jsError = "Failed to open file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }
    char* buffer = new char[maxSize + 1];
    buffer[maxSize] = '\0';
    zip_int64_t len = zip_fread(file, buffer, maxSize);
    if (len == -1) {
        delete[] buffer;
        jsError = "Failed to read file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }

    if (zip_fclose(file)) {
        delete[] buffer;
        jsError = "Failed to close file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }
    ByteStorage* result = new ByteStorage(buffer, len);
    delete[] buffer;
    return result;
}

ByteStorage* ArchiverZip::readFileByIndex(const int64_t fileIndexInZip, const uint64_t maxSize) {
    zip_file* file;
    if (!(file = zip_fopen_index(archive, fileIndexInZip, 0))) {
        // fprintf(stderr, "boese, boese\n");
        // std::cout << "failed to open entry of archive." << zip_strerror(archive) << std::endl;
        // zip_close(archive);
        jsError = "Failed to open file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }
    char* buffer = new char[maxSize + 1];
    buffer[maxSize] = '\0';
    zip_int64_t len = zip_fread(file, buffer, maxSize);
    if (len == -1) {
        delete[] buffer;
        jsError = "Failed to read file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }

    if (zip_fclose(file)) {
        delete[] buffer;
        jsError = "Failed to close file from zip: ";
        jsError += zip_strerror(archive);
        return nullptr;
    }
    ByteStorage* result = new ByteStorage(buffer, len);
    delete[] buffer;
    return result;
}
/*
void* ArchiverZip::getArchive() {

}
*/
