#include <vector>
#include <libbruce/util/be_registry.h>

namespace libbruce { namespace util {

struct factory_rec
{
    std::string scheme;
    be_factory factory;
};

struct spec_parts
{
    std::string scheme;
    std::string location;
    size_t block_size;
    size_t queue_size;
    options_t options;
};

spec_parts parse_spec(const std::string &spec)
{
    spec_parts ret;

    int slash = spec.find("://");
    if (slash == std::string::npos)
        throw factory_error((std::string("No scheme found in block engine spec: ") + spec).c_str());

    ret.scheme = spec.substr(0, slash);

    int semi = spec.find(";");
    if (semi == std::string::npos)
        ret.location = spec.substr(slash + 3);
    else
        ret.location = spec.substr(slash + 3, semi - slash - 3);

    // Parse query parameters
    while (semi != std::string::npos)
    {
        int equals = spec.find("=", semi + 1);
        int next_semi = spec.find(";", semi + 1);

        if (equals != std::string::npos)
        {
            std::string key = spec.substr(semi + 1, equals - semi - 1);
            std::string value = spec.substr(equals + 1, next_semi);

            ret.options[key] = value;
        }

        semi = next_semi;
    }

    ret.block_size = ret.options.get("bs", 1 * 1024 * 1024); // 1 MB
    ret.queue_size = ret.options.get("qs", 256 * 1024); // 256 kB

    return ret;
}

typedef std::vector<factory_rec> factories_t;
factories_t &factories()
{
    static factories_t *factories = new factories_t();
    return *factories;
}

void register_be_factory(const char *scheme, be_factory factory)
{
    factory_rec rec = { std::string(scheme), factory };
    factories().push_back(rec);
}

std::string list_engines()
{
    std::string ret;
    for (factories_t::const_iterator it = factories().begin(); it != factories().end(); ++it)
    {
        if (ret.size())
            ret += ", " + it->scheme;
        else
            ret += it->scheme;
    }
    return ret;
}

be::be_ptr create_be(const std::string &spec)
{
    spec_parts parts = parse_spec(spec);
    for (factories_t::const_iterator it = factories().begin(); it != factories().end(); ++it)
    {
        if (it->scheme == parts.scheme)
            return it->factory(parts.location, parts.block_size, parts.queue_size, parts.options);
    }
    throw factory_error((std::string("No block engine found for scheme: ") + parts.scheme +
                        ". Available: " + list_engines()).c_str());
}

FactoryRegistration::FactoryRegistration(const char *scheme, be_factory factory)
{
    printf("I'm called\n");
    register_be_factory(scheme, factory);
}

}}
