#ifndef BRUCE_BE_BE_H
#define BRUCE_BE_BE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <libbruce/memory.h>

namespace bruce { namespace be {

typedef uint64_t blockid;

struct block_not_found : public std::runtime_error {
    block_not_found(const blockid &id) : std::runtime_error("Block not found: " + std::to_string(id)) { }
};

/**
 * Base block engine class
 */
class be
{
public:
    virtual ~be() {}

    virtual blockid newIdentifier() = 0;
    virtual range get(const blockid &id) = 0;
    virtual void put(const blockid &id, const range &b) = 0;
    virtual void del(const blockid &id) = 0;
};

}}

#endif
