#include "table.h"

#include <algorithm>
#include <cassert>

#include <boost/lexical_cast.hpp>

Table::Table()
{
    m_table.push_back(row_t());
}

Table& Table::put(const std::string &s)
{
    m_table.back().push_back(s);
    return *this;
}

Table& Table::put(size_t x)
{
    put(boost::lexical_cast<std::string>(x));
    return *this;
}

Table &Table::put(int x)
{
    put(boost::lexical_cast<std::string>(x));
    return *this;
}

Table& Table::put(double x)
{
    put(boost::lexical_cast<std::string>(x));
    return *this;
}

Table& Table::perc(double x)
{
    char buffer[10];
    sprintf(buffer, "%.2f%%", x * 100);
    put(std::string(buffer));
    return *this;
}

Table& Table::newline()
{
    m_table.push_back(row_t());
    return *this;
}

void Table::print(std::ostream &os) const
{
    std::vector<size_t> columnSizes;
    for (table_t::const_iterator it = m_table.begin(); it != m_table.end(); ++it)
    {
        for (unsigned i = 0; i < it->size(); i++)
        {
            if (i >= columnSizes.size()) columnSizes.resize(i + 1);
            columnSizes[i] = std::max(columnSizes[i], (*it)[i].size() + 2);
            assert(columnSizes[i] >= (*it)[i].size());
        }
    }

    for (table_t::const_iterator it = m_table.begin(); it != m_table.end(); ++it)
    {
        for (unsigned i = 0; i < it->size(); i++)
        {
            int spaces = columnSizes[i] - (*it)[i].size();
            if (spaces < 0) spaces = 0;

            if (i == 0)
                os << " " << (*it)[i] << std::string(spaces, ' ') << " ";
            else
                os << " " << std::string(spaces, ' ') << (*it)[i] << " ";
        }
        os << std::endl;
    }
}
