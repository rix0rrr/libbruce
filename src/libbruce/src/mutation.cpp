#include <libbruce/mutation.h>

namespace libbruce {

mutation::mutation(maybe_nodeid newRootID)
    : m_success(true), m_newRootID(newRootID)
{
}

mutation::nodes &mutation::deleteList(bool commitSuccess)
{
    if (commitSuccess && m_success)
        return m_obsoleteIDs;
    else
        return m_createdIDs;
}

void mutation::fail(const std::string &reason)
{
    m_success = false;
    m_failureReason = reason;
}

void mutation::setRoot(const maybe_nodeid &id)
{
    m_newRootID = id;
}

void mutation::addCreated(const nodeid_t &id)
{
    m_createdIDs.push_back(id);
}

void mutation::addObsolete(const nodeid_t &id)
{
    m_obsoleteIDs.push_back(id);
}

}
