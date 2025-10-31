/*
  Copyright 2025 Equinor ASA.

  This file is part of the Open Porous Media Project (OPM).

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

#ifndef OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP
#define OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

/// \file Component to describe a collection of region sets.

namespace Opm::data {

    /// Basic information about a collection of region sets.
    ///
    /// In particular, this class tracks the maximum region ID for each
    /// region set registered in the collection.  Common region sets in this
    /// context include the built-in FIPNUM set as well as user defined
    /// region sets named FIP*, such as FIPABC or FIP_UZ.  There is
    /// nevertheless nothing that will prevent including a different
    /// region set such as PVTNUM or KRNUMX in this collection.
    ///
    /// The primary use case for this class is assisting in allocating
    /// region level summary vectors over region sets.  That allocation
    /// requires knowing the maximum expected region ID for each pertinent
    /// region set.
    ///
    /// Constructing a descriptor object is a multi-step process.
    ///
    ///  -# Create the object through the default constructor
    ///  -# Call member function prepareDescriptorSet() to set the stage for
    ///     adding new region sets.
    ///  -# Incorporate one or more region sets through the addRegionSet()
    ///     overload set.
    ///  -# Call member function finaliseDescriptorSet() to analyse the data
    ///     and set up internal data structures.
    ///
    /// Once member function finaliseDescriptorSet() has been called, the
    /// object should be treated as read-only and only its \c const member
    /// functions should be invoked by client code.  Calling member function
    /// prepareDescriptorSet() after finaliseDescriptorSet() is allowed, but
    /// doing so will discard all information collected until that point and
    /// will require re-registering all pertinent region sets.
    ///
    /// The class provides a protected hook through wich derived classes can
    /// extend the protocol--e.g., to include communication in a parallel
    /// context.
    class RegionsetVariableDescriptor
    {
    public:
        /// Default constructor.
        ///
        /// Forms an empty variable descriptor.
        RegionsetVariableDescriptor() = default;

        /// Virtual destructor.
        ///
        /// This class is intended for inheritance.
        virtual ~RegionsetVariableDescriptor() = default;

        /// Copy constructor.
        RegionsetVariableDescriptor(const RegionsetVariableDescriptor&) = default;

        /// Move constructor.
        RegionsetVariableDescriptor(RegionsetVariableDescriptor&&) = default;

        /// Assignment operator.
        RegionsetVariableDescriptor& operator=(const RegionsetVariableDescriptor&) = default;

        /// Move-assignment operator.
        RegionsetVariableDescriptor& operator=(RegionsetVariableDescriptor&&) = default;

        /// Discard all existing information and prepare for analysing new
        /// collection of region sets.
        void prepareDescriptorSet();

        /// Include region set into collection.
        ///
        /// Meaningful only prior to calling finaliseDescriptorSet() and
        /// will throw an exception of type \code std::logic_error \endcode
        /// if called after finaliseDescriptorSet().
        ///
        /// \tparam FwdIt Iterator type.  Needs to be at least a forward
        /// iterator.  Assumed to have an integer value type.
        ///
        /// \param[in] declaredMaxRegionID Run's declared maximum region ID.
        /// Typically entered in the TABDIMS or the REGDIMS keyword.
        ///
        /// \param[in] begin Start of linear sequence of region IDs.
        ///
        /// \param[in] end End of linear sequence of region IDs.
        template <typename FwdIt>
        void addRegionSet(const int declaredMaxRegionID,
                          FwdIt     begin,
                          FwdIt     end)
        {
            const auto maxIdPos = std::max_element(begin, end);
            if (maxIdPos == end) {
                // Empty collection.  Unexpected.
                this->addRegionSet(declaredMaxRegionID);
            }
            else {
                this->addRegionSet(std::max(declaredMaxRegionID,
                                            static_cast<int>(*maxIdPos)));
            }
        }

        /// Include region set into collection.
        ///
        /// Meaningful only prior to calling finaliseDescriptorSet() and
        /// will throw an exception of type \code std::logic_error \endcode
        /// if called after finaliseDescriptorSet().
        ///
        /// \param[in] maxRegionID Maximum region ID in region set.
        void addRegionSet(const int maxRegionID);

        /// Perform post-registration tasks
        ///
        /// Sets up internal CSR-like data structures to enable uniquely
        /// identifying indices corresponding to a single region in a
        /// particular region set.
        ///
        /// Will throw an exception if no region sets have been defined
        /// through member function addRegionSet().
        void finaliseDescriptorSet();

        /// Retrieve value starting index for a particular region set.
        ///
        /// \param[in] Region set index.  Must be in the range [0
        /// ... numRegionSets()-1].
        ///
        /// \return Starting index for the region set defined by the \code
        /// regSet+1 \endcode-th call to addRegionSet().
        std::size_t startIndex(const std::size_t regSet) const
        {
            return this->startPtr_[regSet];
        }

        /// Total number of variable items needed for all regions in all
        /// region sets known to this collection.
        std::size_t numVariableSlots() const
        {
            return this->startPtr_.empty()
                ? std::size_t{0}
                : this->startPtr_.back();
        }

        /// Total number of region sets known to this collection.
        std::size_t numRegionSets() const
        {
            return this->startPtr_.empty()
                ? std::size_t{0}
                : this->startPtr_.size() - 1;
        }

    private:
        /// CSR-like start pointers for all region sets.
        ///
        /// Region 'j' in region set 'i' has variable index \code
        /// startPtr_[i] + j \endcode.
        std::vector<std::size_t> startPtr_{};

        /// Form CSR-like start pointers for all region sets.
        void defineStartPointers();

    protected:
        /// Maximum region IDs in all region sets.
        ///
        /// Nullopt if no region sets have been registered *or* if all
        /// region sets have been internalised through a call to
        /// finaliseDescriptorSet().
        std::optional<std::vector<int>> regsetMaxID_{};

        /// Exchange maximum region set IDs if needed.
        ///
        /// This is a hook for derived classes that know about parallel
        /// execution.  The default implementation, intended/suitable for
        /// sequential runs, does nothing.
        virtual void communicateGlobalRegsetMaxIDs();
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP
