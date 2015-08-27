#ifndef BRUCE_MUTATION_H
#define BRUCE_MUTATION_H

#include <vector>
#include <libbruce/be/be.h>

namespace bruce {

struct mutation
{
    mutation();

    // Public API
    bool success() const { return m_success; }
    const std::string &failureReason() const { return m_failureReason; }
    nodeid_t newRootId() const { return m_newRootId; }
    const std::vector<nodeid_t> &createdIDs() const { return m_createdIDs; }
    const std::vector<nodeid_t> &obsoleteIDs() const { return m_obsoleteIDs; }

    // Implementation API
    void fail(const std::string &reason);
    void setRoot(const nodeid_t &id);
    void addCreated(const nodeid_t &id);
    void addObsolete(const nodeid_t &id);
private:
    bool m_success;
    std::string m_failureReason;
    nodeid_t m_newRootId;
    std::vector<nodeid_t> m_createdIDs;
    std::vector<nodeid_t> m_obsoleteIDs;
};

}

#endif
