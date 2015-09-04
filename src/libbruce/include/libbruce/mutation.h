#ifndef BRUCE_MUTATION_H
#define BRUCE_MUTATION_H

#include <vector>
#include <libbruce/be/be.h>

namespace bruce {

struct mutation
{
    typedef std::vector<nodeid_t> nodes;

    mutation(maybe_nodeid newRootID);

    // Consumer API
    bool success() const { return m_success; }
    const std::string &failureReason() const { return m_failureReason; }
    maybe_nodeid newRootID() const { return m_newRootID; }
    const nodes &createdIDs() const { return m_createdIDs; }
    const nodes &obsoleteIDs() const { return m_obsoleteIDs; }
    // Return the list with nodes to delete
    nodes &deleteList(bool commitSuccess);

    // Library API
    void fail(const std::string &reason);
    void setRoot(const maybe_nodeid &id);
    void addCreated(const nodeid_t &id);
    void addObsolete(const nodeid_t &id);
private:
    bool m_success;
    std::string m_failureReason;
    maybe_nodeid m_newRootID;
    nodes m_createdIDs;
    nodes m_obsoleteIDs;
};

}

#endif
