// Sistema de arquivos em RAM para saves/perfis (player\*). O DragonFS (rom:/) e somente leitura;
// o motor grava e le de volta plyslot*.dat, hiscores etc. Os dados vivem so enquanto o console esta ligado
// (persistir em Controller Pak/SRAM e uma etapa futura).
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

namespace
{
struct RamFile
{
    std::vector<unsigned char> data;
};
std::map<std::string, RamFile> gFiles;

struct Cookie
{
    RamFile* file;
    size_t pos;
    bool canRead, canWrite, append;
};

int CReadImpl(void* c, char* buf, size_t n)
{
    Cookie* k = static_cast<Cookie*>(c);
    const size_t size = k->file->data.size();
    const size_t avail = k->pos < size ? size - k->pos : 0;
    const size_t m = n < avail ? n : avail;
    if (m)
    {
        memcpy(buf, &k->file->data[k->pos], m);
    }
    k->pos += m;
    return static_cast<int>(m);
}

int CWriteImpl(void* c, const char* buf, size_t n)
{
    Cookie* k = static_cast<Cookie*>(c);
    if (k->append)
    {
        k->pos = k->file->data.size();
    }
    if (k->pos + n > k->file->data.size())
    {
        k->file->data.resize(k->pos + n);
    }
    if (n)
    {
        memcpy(&k->file->data[k->pos], buf, n);
    }
    k->pos += n;
    return static_cast<int>(n);
}

// devolve a nova posicao ou -1
long long CSeekImpl(void* c, long long off, int whence)
{
    Cookie* k = static_cast<Cookie*>(c);
    const long long base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? static_cast<long long>(k->pos) : static_cast<long long>(k->file->data.size());
    const long long np = base + off;
    if (np < 0)
    {
        return -1;
    }
    k->pos = static_cast<size_t>(np);
    return np;
}

int CClose(void* c)
{
    delete static_cast<Cookie*>(c);
    return 0;
}

// funopen (BSD, newlib): os tipos dos callbacks variam entre versoes do newlib; deduz-os da declaracao real.
template <class T> struct RdT;
template <class N> struct RdT<int (*)(void*, char*, N)>
{
    static int fn(void* c, char* b, N n) { return CReadImpl(c, b, static_cast<size_t>(n)); }
};
template <class T> struct WrT;
template <class N> struct WrT<int (*)(void*, const char*, N)>
{
    static int fn(void* c, const char* b, N n) { return CWriteImpl(c, b, static_cast<size_t>(n)); }
};
template <class T> struct SkT;
template <class O> struct SkT<O (*)(void*, O, int)>
{
    static O fn(void* c, O off, int w) { return static_cast<O>(CSeekImpl(c, static_cast<long long>(off), w)); }
};

template <class C, class RD, class WR, class SK, class CL>
FILE* OpenWith(FILE* (*fo)(C, RD, WR, SK, CL), Cookie* k)
{
    return fo(k, k->canRead ? RdT<RD>::fn : static_cast<RD>(nullptr), k->canWrite ? WrT<WR>::fn : static_cast<WR>(nullptr),
              SkT<SK>::fn, static_cast<CL>(CClose));
}
} // namespace

extern "C" FILE* gta2_ramfs_open(const std::string& key, const char* mode)
{
    const bool plus = strchr(mode, '+') != nullptr;
    const char m = mode[0];
    auto it = gFiles.find(key);
    if (m == 'r' && it == gFiles.end())
    {
        errno = ENOENT;
        return nullptr;
    }
    RamFile& f = gFiles[key];
    if (m == 'w')
    {
        f.data.clear();
    }
    Cookie* k = new Cookie{&f, 0, m == 'r' || plus, m != 'r' || plus, m == 'a'};
    FILE* fp = OpenWith(&funopen, k);
    if (!fp)
    {
        delete k;
    }
    return fp;
}

extern "C" bool gta2_ramfs_size(const std::string& key, long* size)
{
    auto it = gFiles.find(key);
    if (it == gFiles.end())
    {
        return false;
    }
    *size = static_cast<long>(it->second.data.size());
    return true;
}

extern "C" int gta2_ramfs_remove(const std::string& key)
{
    return gFiles.erase(key) ? 0 : -1;
}
