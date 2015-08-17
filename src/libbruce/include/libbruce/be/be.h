#ifndef BRUCE_BE_BE_H
#define BRUCE_BE_BE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <stdexcept>

#include <libbruce/types.h>
#include <libbruce/memory.h>

namespace bruce { namespace be {

struct block_not_found : public std::runtime_error {
    block_not_found(const nodeident_t &id) : std::runtime_error("Block not found: " + std::to_string(id)) { }
};

/**
 * Base block engine class
 */
class be
{
public:
    virtual ~be() {}

    virtual nodeident_t newIdentifier() = 0;
    virtual memory get(const nodeident_t &id) = 0;
    virtual void put(const nodeident_t &id, const memory &b) = 0;
    virtual void del(const nodeident_t &id) = 0;
};

}}

#endif
