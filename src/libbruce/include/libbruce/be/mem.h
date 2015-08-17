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

    virtual nodeident_t newIdentifier();

    virtual memory get(const nodeident_t &id);
    virtual void put(const nodeident_t &id, const memory &b);
    virtual void del(const nodeident_t &id);
private:
    nodeident_t m_ctr;
    typedef std::map<nodeident_t, std::vector<uint8_t> > blockmap;
    blockmap m_blocks;
};


}}

#endif
