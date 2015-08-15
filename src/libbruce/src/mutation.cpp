#include <libbruce/mutation.h>

namespace bruce {

mutation::mutation()
    : m_success(true)
{
}

void mutation::fail(const std::string &reason)
{
    m_success = false;
    m_failureReason = reason;
}

void mutation::setRoot(const be::blockid &id)
{
    m_newRootId = id;
}

void mutation::addCreated(const be::blockid &id)
{
    m_createdIDs.push_back(id);
}

void mutation::addObsolete(const be::blockid &id)
{
    m_obsoleteIDs.push_back(id);
}

}
