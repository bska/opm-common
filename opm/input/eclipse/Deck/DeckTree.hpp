/*
  Copyright 2021 Statoil ASA.

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

#ifndef DECK_TREE_HPP
#define DECK_TREE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Opm {

// Class DeckTree maintains a minimal relationship between the include files
// in the deck.  In particular, this class supports writing decks with the
// keywords in the correct files.

class DeckTree
{
public:
    DeckTree() = default;
    explicit DeckTree(std::string_view root_file);

    const std::string& parent(std::string_view fname) const;
    const std::string& root() const;

    bool includes(std::string_view parent_file,
                  std::string_view include_file) const;

    bool has_include(std::string_view fname) const;

    void add_include(std::string_view parent_file,
                     std::string_view include_file);

    void add_root(std::string_view fname);

private:
    struct TreeNode
    {
        explicit TreeNode(const std::string& fn);
        TreeNode(const std::string& pn, const std::string& fn);
        void add_include(std::string_view include_file);
        bool includes(const std::string& include_file) const;

        std::string fname{};
        std::optional<std::string> parent{};
        std::unordered_set<std::string> include_files{};
    };

    std::optional<std::string> root_file_{};
    std::unordered_map<std::string, TreeNode> nodes_{};

    std::string add_node(std::string_view fname);
};

} // namespace Opm

#endif // DECK_TREE_HPP
