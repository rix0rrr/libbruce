#ifndef HELPERS_H
#define HELPERS_H

// Macro because we don't have C++11
#define to_string boost::lexical_cast<std::string>
#define IMPLIES(a, b) ((!a) || b)

#endif
