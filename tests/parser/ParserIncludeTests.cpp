/*
  Copyright 2014 by Andreas Lauser

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

#define BOOST_TEST_MODULE ParserTests
#include <boost/test/unit_test.hpp>

#include <boost/version.hpp>

#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Parser/Parser.hpp>
#include <opm/input/eclipse/Parser/ParserKeyword.hpp>

#include <opm/input/eclipse/Deck/Deck.hpp>

#include <opm/input/eclipse/Parser/ParseContext.hpp>
#include <opm/input/eclipse/Parser/ErrorGuard.hpp>
#include <opm/input/eclipse/Parser/InputErrorAction.hpp>

#include <filesystem>
#include <iostream>

namespace {

std::string prefix()
{
#if BOOST_VERSION / 100000 == 1 && BOOST_VERSION / 100 % 1000 < 71
    return boost::unit_test::framework::master_test_suite().argv[2];
#else
    return boost::unit_test::framework::master_test_suite().argv[1];
#endif
}

} // Anonymous namespace

BOOST_AUTO_TEST_CASE(ParserKeyword_includeInvalid)
{
    const auto inputFilePath = std::filesystem::path { prefix() } / "includeInvalid.data";

    Opm::Parser parser;
    Opm::ParseContext parseContext;
    Opm::ErrorGuard errors;

    parseContext.update(Opm::ParseContext::PARSE_MISSING_INCLUDE, Opm::InputErrorAction::THROW_EXCEPTION);
    BOOST_CHECK_THROW(parser.parseFile(inputFilePath.generic_string(), parseContext, errors), Opm::OpmInputError);

    parseContext.update(Opm::ParseContext::PARSE_MISSING_INCLUDE, Opm::InputErrorAction::IGNORE);
    BOOST_CHECK_NO_THROW(parser.parseFile(inputFilePath.generic_string(), parseContext, errors));
}

BOOST_AUTO_TEST_CASE(DATA_FILE_IS_SYMLINK)
{
    const auto inputFilePath = std::filesystem::path { prefix() } /
        "includeSymlinkTestdata/symlink4/path/case.data";

    BOOST_TEST_MESSAGE("Input file: " << inputFilePath.generic_string());

    const auto deck = Opm::Parser{}.parseFile(inputFilePath.string());

    BOOST_CHECK_MESSAGE(deck.hasKeyword("OIL"), "Input deck must have OIL keyword");
    BOOST_CHECK_MESSAGE(! deck.hasKeyword("WATER"), "Input deck must NOT have WATER keyword");
}

BOOST_AUTO_TEST_CASE(Verify_find_includes_Data_file_has_include_that_is_a_symlink)
{
    const auto inputFilePath = std::filesystem::path { prefix() } /
        "includeSymlinkTestdata/symlink2/caseWithIncludedSymlink.data";

    const auto deck = Opm::Parser{}.parseFile(inputFilePath.generic_string());

    BOOST_CHECK_MESSAGE(deck.hasKeyword("OIL"), "Input deck must have OIL keyword");
    BOOST_CHECK_MESSAGE(! deck.hasKeyword("WATER"), "Input deck must NOT have WATER keyword");
}

BOOST_AUTO_TEST_CASE(Verify_find_includes_Data_file_has_include_file_that_again_includes_a_symlink)
{
    const auto inputFilePath = std::filesystem::path { prefix() } /
        "includeSymlinkTestdata/symlink3/case.data";

    const auto deck = Opm::Parser{}.parseFile(inputFilePath.generic_string());

    BOOST_CHECK_MESSAGE(deck.hasKeyword("OIL"), "Input deck must have OIL keyword");
    BOOST_CHECK_MESSAGE(! deck.hasKeyword("WATER"), "Input deck must NOT have WATER keyword");
}

BOOST_AUTO_TEST_CASE(ParserKeyword_includeValid)
{
    const auto inputFilePath = std::filesystem::path { prefix() } /
        "includeValid.data";

    const auto deck = Opm::Parser{}.parseFile(inputFilePath.generic_string());

    BOOST_CHECK_MESSAGE(deck.hasKeyword("OIL"), "Input deck must have OIL keyword");
    BOOST_CHECK_MESSAGE(! deck.hasKeyword("WATER"), "Input deck must NOT have WATER keyword");
}

BOOST_AUTO_TEST_CASE(ParserKeyword_includeWrongCase)
{
    const auto inputFile1Path = std::filesystem::path { prefix() } / "includeWrongCase1.data";
    const auto inputFile2Path = std::filesystem::path { prefix() } / "includeWrongCase2.data";
    const auto inputFile3Path = std::filesystem::path { prefix() } / "includeWrongCase3.data";

    auto parser = Opm::Parser{};

#if HAVE_CASE_SENSITIVE_FILESYSTEM
    // We expect INCLUDE statements to use the same spelling as the file names.
    Opm::ParseContext parseContext;
    Opm::ErrorGuard errors;
    parseContext.update(Opm::ParseContext::PARSE_MISSING_INCLUDE, Opm::InputErrorAction::THROW_EXCEPTION);

    BOOST_CHECK_THROW(parser.parseFile(inputFile1Path.generic_string(), parseContext, errors), Opm::OpmInputError);
    BOOST_CHECK_THROW(parser.parseFile(inputFile2Path.generic_string(), parseContext, errors), Opm::OpmInputError);
    BOOST_CHECK_THROW(parser.parseFile(inputFile3Path.generic_string(), parseContext, errors), Opm::OpmInputError);
#else
    // The include statement will always work regardless of how the
    // capitalization of the included files is wrong.
    BOOST_CHECK_MESSAGE(parser.parseFile(inputFile1Path.generic_string()).hasKeyword("OIL"), "Case 1 must have OIL");
    BOOST_CHECK_MESSAGE(! parser.parseFile(inputFile1Path.generic_string()).hasKeyword("WATER"), "Case 1 must NOT have WATER");

    BOOST_CHECK_MESSAGE(parser.parseFile(inputFile2Path.generic_string()).hasKeyword("OIL"), "Case 2 must have OIL");
    BOOST_CHECK_MESSAGE(! parser.parseFile(inputFile2Path.generic_string()).hasKeyword("WATER"), "Case 1 must NOT have WATER");

    BOOST_CHECK_MESSAGE(parser.parseFile(inputFile3Path.generic_string()).hasKeyword("OIL"), "Case 3 must have OIL");
    BOOST_CHECK_MESSAGE(! parser.parseFile(inputFile3Path.generic_string()).hasKeyword("WATER"), "Case 1 must NOT have WATER");
#endif
}

BOOST_AUTO_TEST_CASE(ParserKeyword_includeFileWithIncorrectlyTerminatedKW)
{
    const auto inputFilePath = std::filesystem::path { prefix() }
        / "includeIncorrectlyTerminatedKW.data";

    const auto keywords_string = std::string { R"(INCLUDE
   ')" + inputFilePath.generic_string() + R"(' /

EQUIL
  2650.00 250.000 2700.00 0.00 2650.000 0.00 1 1 0 /
  2700.00 253.300 2700.00 0.00 1650.000 0.00 1 1 0 /
  2730.00 300.000 2725.00 0.00 1650.000 0.00 1 1 0 /
  2730.00 300.000 2715.00 0.00 1650.000 0.00 1 1 0 /
)" };

    BOOST_CHECK_THROW(Opm::Parser{}.parseString(keywords_string), Opm::OpmInputError);
}
