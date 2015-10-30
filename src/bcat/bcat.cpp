#include <libbruce/bruce.h>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/copy.hpp>

// Cheekily access the private API of the library
#include "../libbruce/src/serializing.h"

#include <fstream>

namespace io = boost::iostreams;

using namespace libbruce;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: bcat FILE" << std::endl;
        return 1;
    }

    std::fstream file(argv[1], std::fstream::in | std::fstream::binary | std::fstream::ate);
    size_t size = file.tellg();
    file.seekg(0);

    mempage mem(size);
    io::basic_array_sink<char> memstream((char*)mem.ptr(), mem.size());
    io::copy(file, memstream);

    node_ptr node = ParseNode(mem, tree<std::string, std::string>::fns);

    std::cout << *node << std::endl;

    return 0;
}
