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
    be::blockid newRootId() const { return m_newRootId; }
    const std::vector<be::blockid> &createdIDs() const { return m_createdIDs; }
    const std::vector<be::blockid> &obsoleteIDs() const { return m_obsoleteIDs; }

    // Implementation API
    void fail(const std::string &reason);
    void setRoot(const be::blockid &id);
    void addCreated(const be::blockid &id);
    void addObsolete(const be::blockid &id);
private:
    bool m_success;
    std::string m_failureReason;
    be::blockid m_newRootId;
    std::vector<be::blockid> m_createdIDs;
    std::vector<be::blockid> m_obsoleteIDs;
};

}

#endif
