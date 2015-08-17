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
    nodeident_t newRootId() const { return m_newRootId; }
    const std::vector<nodeident_t> &createdIDs() const { return m_createdIDs; }
    const std::vector<nodeident_t> &obsoleteIDs() const { return m_obsoleteIDs; }

    // Implementation API
    void fail(const std::string &reason);
    void setRoot(const nodeident_t &id);
    void addCreated(const nodeident_t &id);
    void addObsolete(const nodeident_t &id);
private:
    bool m_success;
    std::string m_failureReason;
    nodeident_t m_newRootId;
    std::vector<nodeident_t> m_createdIDs;
    std::vector<nodeident_t> m_obsoleteIDs;
};

}

#endif
