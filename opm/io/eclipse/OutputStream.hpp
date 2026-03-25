/*
  Copyright (c) 2019 Equinor ASA
  Copyroght (c) 2026 OPM-OP AS

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

#ifndef OPM_IO_OUTPUTSTREAM_HPP_INCLUDED
#define OPM_IO_OUTPUTSTREAM_HPP_INCLUDED

#include <opm/io/eclipse/PaddedOutputString.hpp>
#include <opm/common/utility/TimeService.hpp>

#include <array>
#include <chrono>
#include <ios>
#include <memory>
#include <string>
#include <vector>

namespace Opm { namespace EclIO {

    class EclOutput;

}} // namespace Opm::EclIO

namespace Opm { namespace EclIO { namespace OutputStream {

    struct Formatted { bool set; };
    struct Unified   { bool set; };

    /// Abstract representation of an ECLIPSE-style result set.
    struct ResultSet
    {
        /// Output directory.  Commonly "." or location of run's .DATA file.
        std::string outputDir;

        /// Base name of simulation run.
        std::string baseName;
    };

    /// File manager for "init" output streams.
    class Init
    {
    public:
        /// Constructor.
        ///
        /// Opens file stream for writing.
        ///
        /// \param[in] rset Output directory and base name of output stream.
        ///
        /// \param[in] fmt Whether or not to create formatted output files.
        explicit Init(const ResultSet& rset,
                      const Formatted& fmt);

        ~Init();

        Init(const Init& rhs) = delete;
        Init(Init&& rhs);

        Init& operator=(const Init& rhs) = delete;
        Init& operator=(Init&& rhs);

        /// Write integer data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&      kw,
                   const std::vector<int>& data);

        /// Write boolean data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&       kw,
                   const std::vector<bool>& data);

        /// Write single precision floating point data to underlying
        /// output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&        kw,
                   const std::vector<float>& data);

        /// Write double precision floating point data to underlying
        /// output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&         kw,
                   const std::vector<double>& data);

        /// Write padded character data (8 characters per string)
        /// to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&                        kw,
                   const std::vector<PaddedOutputString<8>>& data);


        /// \param[in] msg Message string (e.g., "STARTSOL").
        void message(const std::string& msg);

    private:
        /// Init file output stream.
        std::unique_ptr<EclOutput> stream_;

        /// Open output stream.
        ///
        /// Writes to \c stream_.
        ///
        /// \param[in] fname Filename of new output stream.
        ///
        /// \param[in] formatted Whether or not to create a
        ///    formatted output file.
        void open(const std::string& fname,
                  const bool         formatted);

        /// Access writable output stream.
        EclOutput& stream();

        /// Implementation function for public \c write overload set.
        template <typename T>
        void writeImpl(const std::string&    kw,
                       const std::vector<T>& data);
    };

    /// Abstract base class for restart-like output stream managers.
    ///
    /// Provides a common write interface shared by both the Restart and
    /// Store stream managers, enabling a single save pipeline to serve
    /// both file formats.
    class RestartBase
    {
    public:
        virtual ~RestartBase() = default;

        /// Generate a message string (keyword type 'MESS').
        ///
        /// \param[in] msg Message string (e.g., "STARTSOL").
        virtual void message(const std::string& msg) = 0;

        /// Write integer data.
        virtual void write(const std::string&      kw,
                           const std::vector<int>& data) = 0;

        /// Write boolean data.
        virtual void write(const std::string&       kw,
                           const std::vector<bool>& data) = 0;

        /// Write single-precision floating-point data.
        virtual void write(const std::string&        kw,
                           const std::vector<float>& data) = 0;

        /// Write double-precision floating-point data.
        virtual void write(const std::string&         kw,
                           const std::vector<double>& data) = 0;

        /// Write unpadded string data.
        virtual void write(const std::string&              kw,
                           const std::vector<std::string>& data) = 0;

        /// Write padded character data (8 characters per string).
        virtual void write(const std::string&                        kw,
                           const std::vector<PaddedOutputString<8>>& data) = 0;
    };

    /// File manager for restart output streams.
    class Restart : public RestartBase
    {
    public:
        /// Constructor.
        ///
        /// Opens file stream pertaining to restart of particular report
        /// step and also outputs a SEQNUM record in the case of a unified
        /// output stream.
        ///
        /// Must be called before accessing the stream object through the
        /// stream() member function.
        ///
        /// \param[in] rset Output directory and base name of output stream.
        ///
        /// \param[in] seqnum Sequence number of new report.  One-based
        ///    report step ID.
        ///
        /// \param[in] fmt Whether or not to create formatted output files.
        ///
        /// \param[in] unif Whether or not to create unified output files.
        explicit Restart(const ResultSet& rset,
                         const int        seqnum,
                         const Formatted& fmt,
                         const Unified&   unif);

        ~Restart() override;

        Restart(const Restart& rhs) = delete;
        Restart(Restart&& rhs);

        Restart& operator=(const Restart& rhs) = delete;
        Restart& operator=(Restart&& rhs);

        /// Generate a message string (keyword type 'MESS') in underlying
        /// output stream.
        ///
        /// \param[in] msg Message string (e.g., "STARTSOL").
        void message(const std::string& msg) override;

        /// Write integer data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&      kw,
                   const std::vector<int>& data) override;

        /// Write boolean data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&       kw,
                   const std::vector<bool>& data) override;

        /// Write single precision floating point data to underlying
        /// output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&        kw,
                   const std::vector<float>& data) override;

        /// Write double precision floating point data to underlying
        /// output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&         kw,
                   const std::vector<double>& data) override;

        /// Write unpadded string data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&              kw,
                   const std::vector<std::string>& data) override;

        /// Write padded character data (8 characters per string)
        /// to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&                        kw,
                   const std::vector<PaddedOutputString<8>>& data) override;

    private:
        /// Restart output stream.
        std::unique_ptr<EclOutput> stream_;

        /// Open unified output file and place stream's output indicator
        /// in appropriate location.
        ///
        /// Writes to \c stream_.
        ///
        /// \param[in] fname Filename of output stream.
        ///
        /// \param[in] formatted Whether or not to create a
        ///    formatted output file.
        ///
        /// \param[in] seqnum Sequence number of new report.  One-based
        ///    report step ID.
        void openUnified(const std::string& fname,
                         const bool         formatted,
                         const int          seqnum);

        /// Open new output stream.
        ///
        /// Handles the case of separate output files or unified output file
        /// that does not already exist.  Writes to \c stream_.
        ///
        /// \param[in] fname Filename of new output stream.
        ///
        /// \param[in] formatted Whether or not to create a
        ///    formatted output file.
        void openNew(const std::string& fname,
                     const bool         formatted);

        /// Open existing output file and place stream's output indicator
        /// in appropriate location.
        ///
        /// Writes to \c stream_.
        ///
        /// \param[in] fname Filename of output stream.
        ///
        /// \param[in] writePos Position at which to place stream's output
        ///    indicator.  Use \code streampos{ streamoff{-1} } \endcode to
        ///    place output indicator at end of file (i.e, simple append).
        void openExisting(const std::string&   fname,
                          const bool           formatted,
                          const std::streampos writePos);

        /// Access writable output stream.
        ///
        /// Must not be called prior to \c prepareStep.
        EclOutput& stream();

        /// Implementation function for public \c write overload set.
        template <typename T>
        void writeImpl(const std::string&    kw,
                       const std::vector<T>& data);
    };

    /// File manager for store output streams (.STORE / .FSTORE).
    ///
    /// Writes unified (SEQNUM-framed) output files using the same
    /// record layout as Restart, but with distinct file extensions.
    class Store : public RestartBase
    {
    public:
        /// Constructor.
        ///
        /// Opens, or creates, the unified store file and positions the
        /// output stream for writing.  Emits a SEQNUM record to begin
        /// the new report-step sequence.
        ///
        /// \param[in] rset Output directory and base name of output stream.
        ///
        /// \param[in] seqnum Sequence number of new report.  One-based
        ///    report step ID.
        ///
        /// \param[in] fmt Whether or not to create formatted output files.
        explicit Store(const ResultSet& rset,
                       const int        seqnum,
                       const Formatted& fmt);

        ~Store() override;

        Store(const Store& rhs) = delete;
        Store(Store&& rhs);

        Store& operator=(const Store& rhs) = delete;
        Store& operator=(Store&& rhs);

        /// Generate a message string (keyword type 'MESS').
        ///
        /// \param[in] msg Message string (e.g., "STARTSOL").
        void message(const std::string& msg) override;

        /// Write integer data.
        void write(const std::string&      kw,
                   const std::vector<int>& data) override;

        /// Write boolean data.
        void write(const std::string&       kw,
                   const std::vector<bool>& data) override;

        /// Write single-precision floating-point data.
        void write(const std::string&        kw,
                   const std::vector<float>& data) override;

        /// Write double-precision floating-point data.
        void write(const std::string&         kw,
                   const std::vector<double>& data) override;

        /// Write unpadded string data.
        void write(const std::string&              kw,
                   const std::vector<std::string>& data) override;

        /// Write padded character data (8 characters per string).
        void write(const std::string&                        kw,
                   const std::vector<PaddedOutputString<8>>& data) override;

    private:
        /// Store file output stream.
        std::unique_ptr<EclOutput> stream_;

        /// Open unified store file and position stream for the given seqnum.
        void openUnified(const std::string& fname,
                         const bool         formatted,
                         const int          seqnum);

        /// Create a new store file.
        void openNew(const std::string& fname,
                     const bool         formatted);

        /// Open existing store file and position at \p writePos.
        void openExisting(const std::string&   fname,
                          const bool           formatted,
                          const std::streampos writePos);

        /// Access writable output stream.
        EclOutput& stream();

        /// Implementation function for public \c write overload set.
        template <typename T>
        void writeImpl(const std::string&    kw,
                       const std::vector<T>& data);
    };

    /// File manager for RFT output streams
    class RFT
    {
    public:
        struct OpenExisting { bool set; };

        /// Constructor.
        ///
        /// Opens file stream for writing.
        ///
        /// \param[in] rset Output directory and base name of output stream.
        ///
        /// \param[in] fmt Whether or not to create formatted output files.
        ///
        /// \param[in] existing Whether or not to open an existing output file.
        explicit RFT(const ResultSet&    rset,
                     const Formatted&    fmt,
                     const OpenExisting& existing);

        ~RFT();

        RFT(const RFT& rhs) = delete;
        RFT(RFT&& rhs);

        RFT& operator=(const RFT& rhs) = delete;
        RFT& operator=(RFT&& rhs);

        /// Write integer data to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&      kw,
                   const std::vector<int>& data);

        /// Write single precision floating point data to underlying
        /// output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&        kw,
                   const std::vector<float>& data);

        /// Write padded character data (8 characters per string)
        /// to underlying output stream.
        ///
        /// \param[in] kw Name of output vector (keyword).
        ///
        /// \param[in] data Output values.
        void write(const std::string&                        kw,
                   const std::vector<PaddedOutputString<8>>& data);

    private:
        /// Init file output stream.
        std::unique_ptr<EclOutput> stream_;

        /// Open output stream.
        ///
        /// Writes to \c stream_.
        ///
        /// \param[in] fname Filename of new output stream.
        ///
        /// \param[in] formatted Whether or not to create a
        ///    formatted output file.
        ///
        /// \param[in] existing Whether or not to open an
        ///    existing output file (mode ios_base::app).
        void open(const std::string& fname,
                  const bool         formatted,
                  const bool         existing);

        /// Access writable output stream.
        EclOutput& stream();

        /// Implementation function for public \c write overload set.
        template <typename T>
        void writeImpl(const std::string&    kw,
                       const std::vector<T>& data);
    };

    class SummarySpecification
    {
    public:
        using StartTime = time_point;

        enum class UnitConvention
        {
            Metric = 1,
            Field  = 2,
            Lab    = 3,
            Pvt_M  = 4,
        };

        struct RestartSpecification
        {
            std::string root;
            int step;
        };

        class Parameters
        {
        public:
            void add(const std::string& keyword,
                     const std::string& wgname,
                     const int          num,
                     const std::string& unit);

            friend class SummarySpecification;

        private:
            std::vector<PaddedOutputString<8>> keywords{};
            std::vector<PaddedOutputString<8>> wgnames{};
            std::vector<int>                   nums{};
            std::vector<PaddedOutputString<8>> units{};
        };

        explicit SummarySpecification(const ResultSet&            rset,
                                      const Formatted&            fmt,
                                      const UnitConvention        uconv,
                                      const std::array<int,3>&    cartDims,
                                      const RestartSpecification& restart,
                                      const StartTime             start,
                                      const StartTime             computeStart);

        ~SummarySpecification();

        SummarySpecification(const SummarySpecification& rhs) = delete;
        SummarySpecification(SummarySpecification&& rhs);

        SummarySpecification& operator=(const SummarySpecification& rhs) = delete;
        SummarySpecification& operator=(SummarySpecification&& rhs);

        /// \brief Write SMSPEC file
        ///
        /// \param[in] params Summary vector definitions
        /// \param[in] simulationFinished Whether the simulation has finished (i.e.
        ///                           this is the last written for it)
        /// \param[in] currentStep        Index of the current report step
        /// \param[in] basic              The value assigned to BASIC in RPRTRST
        void write(const Parameters& params, const bool simulationFinished,
                   const int currentStep, const int basic);

    private:
        int unit_;
        int restartStep_;
        std::array<int,3> cartDims_;
        StartTime startDate_;
        /// \brief When the simulation started
        StartTime computeStart_;
        std::vector<PaddedOutputString<8>> restart_;

        /// Summary specification (SMSPEC) file output stream.
        std::unique_ptr<EclOutput> stream_;

        void rewindStream();
        void flushStream();

        EclOutput& stream();
    };

    std::unique_ptr<EclOutput>
    createSummaryFile(const ResultSet& rset,
                      const int        seqnum,
                      const Formatted& fmt,
                      const Unified&   unif);

    /// Derive filename corresponding to output stream of particular result
    /// set, with user-specified file extension.
    ///
    /// Low-level string concatenation routine that does not know specific
    /// relations between base names and file extensions.  Handles details
    /// of base name ending in a period (full stop) or having a name that
    /// might otherwise appear to contain a file extension (e.g., CASE.01).
    ///
    /// \param[in] rsetDescriptor Output directory and base name of result set.
    ///
    /// \param[in] ext Filename extension.
    ///
    /// \return outputDir/baseName.ext
    std::string outputFileName(const ResultSet&   rsetDescriptor,
                               const std::string& ext);

}}} // namespace Opm::EclIO::OutputStream

#endif //  OPM_IO_OUTPUTSTREAM_HPP_INCLUDED
