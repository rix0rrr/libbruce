#pragma once
#ifndef BRUCE_BE_BE_H
#define BRUCE_BE_BE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <stdexcept>

#include <boost/range.hpp>
#include <boost/lexical_cast.hpp>

#include <libbruce/types.h>
#include <libbruce/mempage.h>

namespace libbruce { namespace be {

struct be_error : public std::runtime_error {
    be_error(const char *what) : std::runtime_error(what) { }
};

struct block_not_found : public std::runtime_error {
    block_not_found(const nodeid_t &id) : std::runtime_error("Block not found: " + boost::lexical_cast<std::string>(id)) { }
};

/**
 * Request to write a block
 */
struct putblock_t
{
    putblock_t(nodeid_t id, const mempage &mem) : id(id), mem(mem), success(false) { }

    nodeid_t id;
    mempage mem;
    bool success;
    std::string failureReason;
};

typedef std::vector<putblock_t> putblocklist_t;

/**
 * Request to delete a block
 */
struct delblock_t
{
    delblock_t(nodeid_t id) : id(id), success(false) { }

    nodeid_t id;
    bool success;
    std::string failureReason;
};

typedef std::vector<delblock_t> delblocklist_t;

/**
 * Base block engine class
 */
class be
{
public:
    virtual ~be() {}

    virtual nodeid_t id(const mempage &block) = 0;
    virtual mempage get(const nodeid_t &id) = 0;
    virtual void put_all(putblocklist_t &blocklist) = 0;
    virtual void del_all(delblocklist_t &ids) = 0;
    virtual uint32_t maxBlockSize() = 0;
};

}}

#endif
