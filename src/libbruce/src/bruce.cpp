#include <libbruce/bruce.h>

#include <algorithm>

namespace libbruce {

bool doFinish(be::be &blockEngine, mutation &mut, bool success)
{
    mutation::nodes &ns = mut.deleteList(success);
    be::delblocklist_t dels;
    dels.reserve(ns.size());

    for (mutation::nodes::iterator it = ns.begin(); it != ns.end(); ++it)
    {
        dels.push_back(be::delblock_t(*it));
    }

    try
    {
        blockEngine.del_all(dels);

        bool allDeleted = true;
        for (be::delblocklist_t::iterator it = dels.begin(); it != dels.end(); ++it)
        {
            if (it->success)
                ns.erase(std::find(ns.begin(), ns.end(), it->id));
            else
                allDeleted = false;
        }

        return allDeleted;
    }
    catch (std::runtime_error &e)
    {
        mut.fail(e.what());
        return false;
    }
}

}
