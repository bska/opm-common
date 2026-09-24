/*
  Copyright 2026 Equinor ASA.

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

#ifndef OPM_CONNECTIONORDERING_HPP_INCLUDED
#define OPM_CONNECTIONORDERING_HPP_INCLUDED

#include <opm/input/eclipse/Schedule/Well/Connection.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace Opm {
    class WellMatcher;
} // namespace Opm

namespace Opm {

    /// \brief Manages the ordering of well connections based on specified patterns.
    class ConnectionOrdering
    {
    public:
        /// \brief Default constructor.
        ConnectionOrdering() = default;

        /// \brief Default destructor.
        ~ConnectionOrdering() = default;

        /// \brief Copy constructor.
        ///
        /// \param[in] rhs Source object.
        ConnectionOrdering(const ConnectionOrdering& rhs) = default;

        /// \brief Assignment operator.
        ///
        /// \param[in] rhs Source object.
        ///
        /// \return Reference to this object.
        ConnectionOrdering& operator=(const ConnectionOrdering& rhs) = default;

        /// \brief Move constructor.
        ///
        /// \param[in] rhs Source object.  Left in a valid but unspecified state on exit.
        ConnectionOrdering(ConnectionOrdering&& rhs) = default;

        /// \brief Move assignment operator.
        ///
        /// \param[in] rhs Source object.  Left in a valid but unspecified state on exit.
        ///
        /// \return Reference to this object.
        ConnectionOrdering& operator=(ConnectionOrdering&& rhs) = default;

        /// \brief Registers a connection order for a particular set of wells matching the given
        /// pattern.
        ///
        /// The typical source of the input data is the SCHEDULE section COMPORD keyword.
        ///
        /// \param[in] order The connection order to set.
        ///
        /// \param[in] pattern The pattern associated with the connection order.  Expected to be
        /// a well name like 'PROD', a well template like 'PROD*', a well list like '*P', or a
        /// a well list template like '*P*'.
        ///
        /// \return True if the input resulted in an update, false otherwise.
        bool update(Connection::Order order, const std::string& pattern);

        /// \brief Determines the connection order for a specific well based on the registered
        /// patterns.
        ///
        /// \note If no specific pattern matches, including for any unregistered wells,
        /// the default connection order (TRACK) is used.
        ///
        /// \param[in] wellName The name of the well.
        ///
        /// \param[in] wellMatcher The well matcher used to match the well name against patterns.
        ///
        /// \return The connection order for the specified, named well.
        Connection::Order getConnectionOrder(const std::string& wellName,
                                             const WellMatcher& wellMatcher) const;

        /// \brief Creates a test object for serialization purposes.
        ///
        /// \return A ConnectionOrdering object suitable for serialization testing.
        static ConnectionOrdering serializationTestObject();

        /// \brief Equality predicate.
        ///
        /// \param[in] that The object to compare with.
        ///
        /// \return Whether or not \c *this is the same as \c that.
        bool operator==(const ConnectionOrdering& that) const noexcept;

        /// Convert between byte array and object representation.
        ///
        /// \tparam Serializer Byte array conversion protocol.
        ///
        /// \param[in,out] serializer Byte array conversion object.
        template <typename Serializer>
        void serializeOp(Serializer& serialize)
        {
            serialize(this->patterns_);
            serialize(this->hasAnyNonDefaultPatterns_);
        }

    private:
        /// Pattern types for non-default connection ordering.
        ///
        /// Well name patterns whose connections are ordered by *TRACK*,
        /// the default, are NOT explicitly listed here.
        enum class PatternType : std::size_t
        {
            /// \brief Pattern type for INPUT connection ordering.
            Input,

            /// \brief Pattern type for DEPTH connection ordering.
            Depth,

            /// \brief Total number of non-default connection ordering patterns.
            ///
            /// \note This value should always be the last in the enum
            /// so that the array of patterns can be correctly sized.
            NumPatterns,
        };

        /// \brief Type alias for an array of vectors of well name patterns corresponding to each
        /// non-default connection ordering type.
        using PatternArray = std::array<std::vector<std::string>,
                                        static_cast<std::size_t>(PatternType::NumPatterns)>;

        /// \brief Array of vectors of well name patterns for each non-default connection ordering
        /// type.
        PatternArray patterns_ {};

        /// \brief Flag indicating whether any non-default patterns have been registered.
        bool hasAnyNonDefaultPatterns_ {false};

        /// \brief Sets the connection order to TRACK for wells matching the given pattern.
        /// \param[in] pattern The pattern associated with the TRACK connection order.
        /// \return True if the input resulted in an update, false otherwise.
        bool setTrackOrdering(const std::string& pattern);

        /// \brief Sets the connection order to INPUT for wells matching the given pattern.
        /// \param[in] pattern The pattern associated with the INPUT connection order.
        /// \return True if the input resulted in an update, false otherwise.
        bool setInputOrdering(const std::string& pattern);

        /// \brief Sets the connection order to DEPTH for wells matching the given pattern.
        /// \param[in] pattern The pattern associated with the DEPTH connection order.
        /// \return True if the input resulted in an update, false otherwise.
        bool setDepthOrdering(const std::string& pattern);

        /// \brief Adds a well name pattern for the specified non-default connection ordering type.
        /// \param[in] type The non-default connection ordering type.
        /// \param[in] pattern The well name pattern to add.
        /// \return True if the pattern was successfully added, false otherwise.
        bool addPattern(PatternType type, const std::string& pattern);

        /// \brief Removes a well name pattern for the specified non-default connection ordering
        /// type.
        /// \param[in] type The non-default connection ordering type.
        /// \param[in] pattern The well name pattern to remove.
        /// \return True if the pattern was successfully removed, false otherwise.
        bool removePattern(PatternType type, const std::string& pattern);

        /// \brief Sets the flag indicating that non-default patterns have been registered.
        /// This should be called whenever a non-default pattern is added or removed.
        void setNonDefaultPatternFlag();

        /// \brief Checks if a well has a specific non-default connection ordering type based on the
        /// given pattern type and well matcher.
        /// \param[in] type The non-default connection ordering type to check.
        /// \param[in] wellName The name of the well to check.
        /// \param[in] wellMatcher The well matcher used to match the well name against patterns.
        /// \return True if the well has the specified non-default connection ordering type, false
        /// otherwise.
        bool wellHasSpecificType(PatternType type,
                                 const std::string& wellName,
                                 const WellMatcher& wellMatcher) const;
    };

} // namespace Opm

#endif // OPM_CONNECTIONORDERING_HPP_INCLUDED
