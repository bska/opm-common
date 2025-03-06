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

  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <opm/input/eclipse/Deck/DeckTree.hpp>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {

    std::string absName(std::string_view fname)
    {
        return std::filesystem::absolute(fname).generic_string();
    }

} // Anonymous namespace

// ---------------------------------------------------------------------------

namespace Opm {

// ---------------------------------------------------------------------------
// class DeckTree::TreeNode
// ---------------------------------------------------------------------------

DeckTree::TreeNode::TreeNode(const std::string& fn)
    : fname(fn)
{}

DeckTree::TreeNode::TreeNode(const std::string& pn, const std::string& fn)
    : fname(fn)
    , parent(pn)
{}

void DeckTree::TreeNode::add_include(std::string_view include_file)
{
    this->include_files.emplace(include_file);
}

bool DeckTree::TreeNode::includes(const std::string& include_file) const
{
    return this->include_files.find(include_file) != this->include_files.end();
}

// ---------------------------------------------------------------------------
// Public interface of class DeckTree
// ---------------------------------------------------------------------------

DeckTree::DeckTree(std::string_view root_file)
{
    this->add_root(root_file);
}

const std::string& DeckTree::parent(std::string_view fname) const
{
    const auto& parent = this->nodes_
        .at(absName(fname)).parent.value();

    return this->nodes_.at(parent).fname;
}

const std::string& DeckTree::root() const
{
    return this->root_file_.value();
}

bool DeckTree::includes(std::string_view parent_file,
                        std::string_view include_file) const
{
    if (!this->root_file_.has_value()) {
        return false;
    }

    return this->nodes_.at(absName(parent_file))
        .includes(absName(include_file));
}

bool DeckTree::has_include(std::string_view fname) const
{
    const auto filePos = this->nodes_.find(absName(fname));

    return (filePos != this->nodes_.end())
        && !filePos->second.include_files.empty();
}

void DeckTree::add_include(std::string_view parent_file,
                           std::string_view include_file)
{
    if (!this->root_file_.has_value()) {
        return;
    }

    const auto parent_fn = absName(parent_file);
    const auto include_fn = absName(include_file);

    this->nodes_.try_emplace(include_fn, parent_fn, include_fn);
    this->nodes_.at(parent_fn).add_include(include_fn);
}

void DeckTree::add_root(std::string_view fname)
{
    if (this->root_file_.has_value()) {
        throw std::logic_error("Root already assigned");
    }

    this->root_file_ = this->add_node(fname);
}

// ---------------------------------------------------------------------------
// Private member functions of class DeckTree
// ---------------------------------------------------------------------------

std::string DeckTree::add_node(std::string_view fname)
{
    auto abs_path = absName(fname);

    this->nodes_.try_emplace(abs_path, abs_path);

    return abs_path;
}

} // namespace Opm
