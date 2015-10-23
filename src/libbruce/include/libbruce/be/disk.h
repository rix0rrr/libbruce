#pragma once
#ifndef BRUCE_BE_DISK_H
#define BRUCE_BE_DISK_H

#include <map>

#include <libbruce/be/be.h>

namespace libbruce { namespace be {

/**
 * Memory block engine
 */
class disk : public be
{
public:
    disk(std::string pathPrefix, uint32_t maxBlockSize, uint32_t editQueueSize=0);

    virtual mempage get(const nodeid_t &id);
    virtual getblockresult_t get_all(const blockidlist_t &ids);
    virtual nodeid_t id(const mempage &block);
    virtual void put_all(putblocklist_t &blocklist);
    virtual void del_all(delblocklist_t &ids);
    virtual uint32_t maxBlockSize();
    virtual uint32_t editQueueSize();
private:
    std::string m_pathPrefix;
    uint32_t m_maxBlockSize;
    uint32_t m_editQueueSize;

    void put_one(putblock_t &block);
    void del_one(delblock_t &block);
};


}}

#endif
