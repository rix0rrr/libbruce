#ifndef BRUCE_BE_MEM_H
#define BRUCE_BE_MEM_H

#include <stdint.h>
#include <map>

#include <libbruce/be/be.h>

namespace bruce { namespace be {

/**
 * Memory block engine
 */
class mem : public be
{
public:
    mem();
    ~mem();

    virtual blockid newIdentifier();

    virtual range get(const blockid &id);
    virtual void put(const blockid &id, const range &b);
    virtual void del(const blockid &id);
private:
    blockid m_ctr;
    typedef std::map<blockid, std::vector<uint8_t> > blockmap;
    blockmap m_blocks;
};


}}

#endif
