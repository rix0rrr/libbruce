#pragma once
#ifndef LIBBRUCE_UTIL_BEREGISTRY_H
#define LIBBRUCE_UTIL_BEREGISTRY_H

#include <libbruce/types.h>
#include <boost/lexical_cast.hpp>
#include <stdexcept>
#include <map>
#include <string>

namespace libbruce { namespace util {

struct factory_error : public std::runtime_error
{
    factory_error(const char *what) : std::runtime_error(what) { }
};

struct options_t : public std::map<std::string, std::string>
{
    template<typename T>
    T get(const std::string &key, T def) const
    {
        options_t::const_iterator it = find(key);
        if (it == end())
            return def;
        return boost::lexical_cast<T>(it->second);
    }
};

typedef be::be_ptr (*be_factory)(const std::string &location, size_t block_size, size_t queue_size, const options_t &options);

/**
 * Register a factory function
 */
void register_be_factory(const char *scheme, be_factory factory);

/**
 * Instantiate a block engine from a spec string
 */
be::be_ptr create_be(const std::string &spec);


struct FactoryRegistration
{
    FactoryRegistration(const char *scheme, be_factory factory);
};


}}

#endif
