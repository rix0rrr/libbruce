#pragma once
#ifndef LIBBRUCE_TABLE_H
#define LIBBRUCE_TABLE_H

#include <stddef.h>
#include <vector>
#include <string>
#include <ostream>

struct Table
{
    typedef std::vector<std::string> row_t;
    typedef std::vector<row_t> table_t;

    Table();

    Table &put(const std::string &s);
    Table &put(size_t x);
    Table &put(int x);
    Table &perc(double x);
    Table &put(double x);
    Table &newline();

    void print(std::ostream &os) const;
private:
    table_t m_table;
};

#endif
