/*
  Copyright 2016  Statoil ASA.

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

#include <opm/parser/eclipse/EclipseState/Grid/GridDims.hpp>

#include <opm/io/eclipse/EGrid.hpp>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>

#include <opm/parser/eclipse/Parser/ParserKeywords/D.hpp> // DIMENS
#include <opm/parser/eclipse/Parser/ParserKeywords/G.hpp> // GDFILE
#include <opm/parser/eclipse/Parser/ParserKeywords/S.hpp> // SPECGRID

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    // keyword must be DIMENS or SPECGRID
    std::array<int, 3> readDims(const Opm::DeckKeyword& keyword)
    {
        const auto& record = keyword.getRecord(0);

        return {{
            record.getItem("NX").get<int>(0),
            record.getItem("NY").get<int>(0),
            record.getItem("NZ").get<int>(0),
        }};
    }
}

namespace Opm {
    GridDims::GridDims()
        : GridDims { 0, 0, 0 }
    {}

    GridDims::GridDims(std::array<int, 3> xyz)
        : GridDims(xyz[0], xyz[1], xyz[2])
    {}

    GridDims::GridDims(size_t nx, size_t ny, size_t nz)
        : m_nx(nx), m_ny(ny), m_nz(nz)
    {}

    GridDims GridDims::serializeObject()
    {
        return {
            std::size_t{1}, std::size_t{2}, std::size_t{3}
        };
    }

    GridDims::GridDims(const Deck& deck)
    {
        if (deck.hasKeyword<ParserKeywords::SPECGRID>()) {
            this->init(deck.getKeyword<ParserKeywords::SPECGRID>());
        }
        else if (deck.hasKeyword<ParserKeywords::DIMENS>()) {
            this->init(deck.getKeyword<ParserKeywords::DIMENS>());
        }
        else if (deck.hasKeyword<ParserKeywords::GDFILE>()) {
            this->binary_init(deck);
        }
        else {
            throw std::invalid_argument {
                "Must have either SPECGRID or DIMENS to indicate grid dimensions"
            };
        }
    }

    size_t GridDims::getNX() const
    {
        return this->m_nx;
    }

    size_t GridDims::getNY() const
    {
        return this->m_ny;
    }

    size_t GridDims::getNZ() const
    {
        return this->m_nz;
    }

    size_t GridDims::operator[](int dim) const
    {
        switch (dim) {
        case 0: return this->getNX();
        case 1: return this->getNY();
        case 2: return this->getNZ();
        }

        throw std::invalid_argument {
            "Invalid argument dim: " + std::to_string(dim)
        };
    }

    std::array<int, 3> GridDims::getNXYZ() const
    {
        return std::array<int, 3> {{
            static_cast<int>(this->getNX()),
            static_cast<int>(this->getNY()),
            static_cast<int>(this->getNZ()),
        }};
    }

    size_t GridDims::getGlobalIndex(size_t i, size_t j, size_t k) const
    {
        return i + this->getNX()*(j + this->getNY()*k);
    }

    std::array<int, 3> GridDims::getIJK(size_t globalIndex) const
    {
        const auto i = globalIndex % this->getNX();  globalIndex /= this->getNX();
        const auto j = globalIndex % this->getNY();
        const auto k = globalIndex / this->getNY();

        return std::array<int,3> {{
            static_cast<int>(i), static_cast<int>(j), static_cast<int>(k)
        }};
    }

    size_t GridDims::getCartesianSize() const
    {
        return this->getNX() * this->getNY() * this->getNZ();
    }

    void GridDims::assertGlobalIndex(size_t globalIndex) const
    {
        if (globalIndex >= getCartesianSize()) {
            throw std::invalid_argument("input index above valid range");
        }
    }

    void GridDims::assertIJK(size_t i, size_t j, size_t k) const
    {
        if ((i >= this->getNX()) ||
            (j >= this->getNY()) ||
            (k >= this->getNZ()))
        {
            throw std::invalid_argument("input index above valid range");
        }
    }

    void GridDims::init(const DeckKeyword& keyword)
    {
        auto dims = readDims(keyword);

        this->m_nx = dims[0];
        this->m_ny = dims[1];
        this->m_nz = dims[2];
    }

    void GridDims::binary_init(const Deck& deck)
    {
        const auto gdfile = deck.getKeyword<ParserKeywords::GDFILE>()
            .getRecord(0).getItem<ParserKeywords::GDFILE::filename>()
            .get<std::string>(0);

        const auto dimens =
            EclIO::EGrid { deck.makeDeckPath(gdfile) }.dimension();

        this->m_nx = dimens[0];
        this->m_ny = dimens[1];
        this->m_nz = dimens[2];
    }

    bool GridDims::operator==(const GridDims& data) const
    {
        return this->getNXYZ() == data.getNXYZ();
    }
}
