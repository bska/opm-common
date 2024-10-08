/*
  Copyright 2016 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_OUTPUT_DATA_SOLUTION_HPP
#define OPM_OUTPUT_DATA_SOLUTION_HPP

#include <opm/output/data/Cells.hpp>

#include <opm/input/eclipse/Units/UnitSystem.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Opm { namespace data {

class Solution : public std::map<std::string, data::CellData>
{
    using Base = std::map<std::string, data::CellData>;

    public:
        Solution() = default;
        explicit Solution( bool si );
        using Base::map;
        using Base::insert;

        bool has( const std::string& ) const;

        /*
         * Get the data field of the struct matching the requested key. Will
         * throw std::out_of_range if they key does not exist.
         */
        template<class T>
        std::vector<T>& data(const std::string& );

        template<class T>
        const std::vector<T>& data(const std::string& ) const;

        std::pair<iterator, bool> insert(std::string name,
                                         UnitSystem::measure,
                                         std::vector<double>,
                                         TargetType );

        std::pair<iterator, bool> insert(std::string name,
                                         UnitSystem::measure,
                                         std::vector<float>,
                                         TargetType );

        std::pair<iterator, bool> insert(std::string name,
                                         std::vector<int>,
                                         TargetType );

        void convertToSI( const UnitSystem& );
        void convertFromSI( const UnitSystem& );

        template<class Serializer>
        void serializeOp(Serializer& serializer)
        {
          serializer(static_cast<Base&>(*this));
          serializer(si);
        }

        static Solution serializationTestObject()
        {
            Solution sol;
            sol.si = false;
            sol.insert({"test_data", CellData::serializationTestObject()});

            return sol;
        }

    private:
        bool si = true;
};

}} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_SOLUTION_HPP
