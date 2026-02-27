"""Tests for bin/keyword_to_md.py"""

import os
import sys
import tempfile
import unittest

# Resolve the project root relative to this test file so that paths remain
# correct regardless of the current working directory.
_TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.abspath(os.path.join(_TESTS_DIR, "..", ".."))

# Allow importing keyword_to_md from the bin directory
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "bin"))
import keyword_to_md  # noqa: E402


class TestKeywordToMarkdown(unittest.TestCase):

    # ------------------------------------------------------------------ helpers

    def _md(self, data: dict) -> str:
        return keyword_to_md.keyword_to_markdown(data)

    # ---------------------------------------------------------- basic structure

    def test_title_is_keyword_name(self):
        md = self._md({"name": "WELSPECS", "sections": ["SCHEDULE"]})
        self.assertIn("# WELSPECS\n", md)

    def test_sections_listed(self):
        md = self._md({"name": "AQUCT", "sections": ["GRID", "PROPS"]})
        self.assertIn("## Sections", md)
        self.assertIn("- GRID", md)
        self.assertIn("- PROPS", md)

    def test_description_used_when_present(self):
        md = self._md({"name": "X", "sections": [], "description": "My description."})
        self.assertIn("My description.", md)
        self.assertNotIn("TODO", md.split("## Notes")[0])

    def test_comment_used_as_fallback(self):
        md = self._md({"name": "X", "sections": [], "comment": "E300 only"})
        self.assertIn("E300 only", md)

    def test_todo_placeholder_when_no_description(self):
        md = self._md({"name": "X", "sections": []})
        self.assertIn("TODO", md.split("## Notes")[0])

    # ------------------------------------------------------------------- items

    def test_items_table_present(self):
        data = {
            "name": "DATES",
            "sections": ["SCHEDULE"],
            "items": [
                {"name": "DAY", "value_type": "INT"},
                {"name": "MONTH", "value_type": "STRING"},
            ],
        }
        md = self._md(data)
        self.assertIn("## Items", md)
        self.assertIn("`DAY`", md)
        self.assertIn("`MONTH`", md)
        self.assertIn("INT", md)
        self.assertIn("STRING", md)

    def test_items_default_and_dimension(self):
        data = {
            "name": "WELSPECS",
            "sections": ["SCHEDULE"],
            "items": [
                {"name": "REF_DEPTH", "value_type": "DOUBLE",
                 "default": 0.0, "dimension": "Length"},
            ],
        }
        md = self._md(data)
        self.assertIn("0.0", md)
        self.assertIn("Length", md)

    def test_items_comment_in_description_column(self):
        data = {
            "name": "WELSPECS",
            "sections": ["SCHEDULE"],
            "items": [
                {"name": "FRONTSIM1", "value_type": "STRING",
                 "comment": "Not applicable"},
            ],
        }
        md = self._md(data)
        self.assertIn("Not applicable", md)

    # ------------------------------------------------------------------ records

    def test_records_section(self):
        data = {
            "name": "PYACTION",
            "sections": ["SCHEDULE"],
            "size": 2,
            "records": [
                [{"name": "NAME", "value_type": "STRING"}],
                [{"name": "FILENAME", "value_type": "STRING"}],
            ],
        }
        md = self._md(data)
        self.assertIn("## Records", md)
        self.assertIn("Record 1", md)
        self.assertIn("Record 2", md)
        self.assertIn("`NAME`", md)
        self.assertIn("`FILENAME`", md)

    def test_alternating_records_section(self):
        data = {
            "name": "PVTWSALT",
            "sections": ["PROPS"],
            "alternating_records": [
                [{"name": "P_REF", "value_type": "DOUBLE"}],
                [{"name": "DATA", "value_type": "DOUBLE"}],
            ],
        }
        md = self._md(data)
        self.assertIn("## Alternating Records", md)
        self.assertIn("`P_REF`", md)
        self.assertIn("`DATA`", md)

    def test_records_set_section(self):
        data = {
            "name": "UDT",
            "sections": ["SCHEDULE"],
            "records_set": [
                [{"name": "TABLE_NAME", "value_type": "STRING"}],
                [{"name": "TABLE_VALUES", "value_type": "DOUBLE"}],
            ],
        }
        md = self._md(data)
        self.assertIn("## Records Set", md)
        self.assertIn("`TABLE_NAME`", md)

    # -------------------------------------------------------------------- data

    def test_data_keyword(self):
        data = {
            "name": "SMULTZ",
            "sections": ["GRID"],
            "data": {"value_type": "DOUBLE"},
        }
        md = self._md(data)
        self.assertIn("## Data", md)
        self.assertIn("`DOUBLE`", md)

    def test_data_keyword_with_dimension(self):
        data = {
            "name": "BIOTCOEF",
            "sections": ["GRID"],
            "data": {"value_type": "DOUBLE", "dimension": "1"},
        }
        md = self._md(data)
        self.assertIn("Dimension: 1.", md)

    # ---------------------------------------------------------------- sizing

    def test_fixed_size(self):
        md = self._md({"name": "ACTDIMS", "sections": ["RUNSPEC"], "size": 1})
        self.assertIn("## Sizing", md)
        self.assertIn("Fixed number of records: 1", md)

    def test_keyword_driven_size(self):
        data = {
            "name": "PVTWSALT",
            "sections": ["PROPS"],
            "size": {"keyword": "TABDIMS", "item": "NTPVT"},
        }
        md = self._md(data)
        self.assertIn("`NTPVT`", md)
        self.assertIn("`TABDIMS`", md)

    def test_num_tables(self):
        data = {
            "name": "PVTGW",
            "sections": ["PROPS"],
            "num_tables": {"keyword": "TABDIMS", "item": "NTPVT"},
        }
        md = self._md(data)
        self.assertIn("## Sizing", md)
        self.assertIn("Number of tables", md)

    # ------------------------------------------------------------- dependencies

    def test_requires(self):
        data = {
            "name": "AQUCT",
            "sections": ["GRID"],
            "requires": ["AQUDIMS"],
        }
        md = self._md(data)
        self.assertIn("## Dependencies", md)
        self.assertIn("`AQUDIMS`", md)

    def test_prohibits(self):
        data = {
            "name": "BIOTCOEF",
            "sections": ["GRID"],
            "prohibits": ["POELCOEF"],
        }
        md = self._md(data)
        self.assertIn("## Dependencies", md)
        self.assertIn("`POELCOEF`", md)

    # --------------------------------------------------------- alternative names

    def test_deck_names(self):
        data = {
            "name": "BLOCK_PROBE300",
            "sections": ["SUMMARY"],
            "deck_names": ["BTEMP"],
        }
        md = self._md(data)
        self.assertIn("## Alternative Names", md)
        self.assertIn("`BTEMP`", md)

    # ------------------------------------------------------------------- code

    def test_code_block_keyword(self):
        data = {
            "name": "DYNAMICR",
            "sections": ["SCHEDULE"],
            "code": {"end": "ENDDYN"},
        }
        md = self._md(data)
        self.assertIn("## Syntax", md)
        self.assertIn("`ENDDYN`", md)

    # ----------------------------------------------------------- notes section

    def test_notes_always_present(self):
        md = self._md({"name": "X", "sections": []})
        self.assertIn("## Notes", md)

    # ---------------------------------------------------- CLI: stdout and file

    def test_main_stdout(self):
        kw_file = os.path.join(
            _PROJECT_ROOT,
            "opm", "input", "eclipse", "share", "keywords",
            "000_Eclipse100", "W", "WELSPECS",
        )
        # main() should succeed (return 0) and not raise
        ret = keyword_to_md.main([kw_file])
        self.assertEqual(ret, 0)

    def test_main_file_output(self):
        kw_file = os.path.join(
            _PROJECT_ROOT,
            "opm", "input", "eclipse", "share", "keywords",
            "000_Eclipse100", "S", "SMULTZ",
        )
        with tempfile.NamedTemporaryFile(suffix=".md", delete=False) as tmp:
            tmp_name = tmp.name
        try:
            ret = keyword_to_md.main([kw_file, tmp_name])
            self.assertEqual(ret, 0)
            with open(tmp_name, encoding="utf-8") as fh:
                content = fh.read()
            self.assertIn("# SMULTZ", content)
        finally:
            os.unlink(tmp_name)

    def test_main_missing_file_returns_error(self):
        ret = keyword_to_md.main(["/nonexistent/path/KEYWORD"])
        self.assertNotEqual(ret, 0)

    def test_main_invalid_json_returns_error(self):
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False
        ) as tmp:
            tmp.write("{ not valid json }")
            tmp_name = tmp.name
        try:
            ret = keyword_to_md.main([tmp_name])
            self.assertNotEqual(ret, 0)
        finally:
            os.unlink(tmp_name)


if __name__ == "__main__":
    unittest.main()
