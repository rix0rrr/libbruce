#ifndef BRUCE_BE_MEM_H
#define BRUCE_BE_MEM_H

#include <stdint.h>
#include <map>

#include <libbruce/be/be.h>

namespace libbruce { namespace be {

/**
 * Memory block engine
 */
class mem : public be
{
public:
    mem(uint32_t maxBlockSize);
    ~mem();

    virtual void newIdentifiers(int n, std::vector<nodeid_t> *out);

    nodeid_t blockCount() const;

    virtual memory get(const nodeid_t &id);
    virtual void put_all(putblocklist_t &blocklist);
    virtual void del_all(delblocklist_t &ids);
    virtual uint32_t maxBlockSize();
private:
    nodeid_t m_ctr;
    typedef std::map<nodeid_t, memory> blockmap;
    blockmap m_blocks;
    uint32_t m_maxBlockSize;
};


}}

#endif
