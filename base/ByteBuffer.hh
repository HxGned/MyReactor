#ifndef __BYTE_BUFFER_HH__
#define __BYTE_BUFFER_HH__

#include <sys/types.h>
#include <vector>

using std::vector;

namespace base {

const int kInitialBufferSize = 4096;

class ByteBuffer {
public:
    ByteBuffer();
    ~ByteBuffer();

    size_t ReadableBytes();
    size_t WritableBytes();
    size_t InternalBufferCapacity();

    ssize_t ReadFd(int fd, int *saveErrno);

    void MakeRoom(size_t len);
    void Append(char *ptr, size_t len);
    void Retrieve(size_t len);

    char* Begin();
    char* ReadBegin();
    char* WriteBegin();
private:
    vector<char> _buf;

    size_t _readIndex;
    size_t _writeIndex;
};

};

#endif