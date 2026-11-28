# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Portable checks for the AU discovery gate; macOS performs actual validation."""
import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('validate_audio_unit',
                                             Path(__file__).resolve().parents[1] / 'scripts/validate_audio_unit.py')
au = importlib.util.module_from_spec(spec)
spec.loader.exec_module(au)


class AudioUnitValidationTests(unittest.TestCase):
    def fixture(self):
        return {'CFBundleIdentifier': au.BUNDLE_ID, 'CFBundleExecutable': au.EXECUTABLE_NAME,
                'AudioComponents': [{'type': 'aumi', 'subtype': 'Tbs1', 'manufacturer': 'Tmlz',
                                     'factoryFunction': 'The_Three_Body_SolutionAUFactory'}]}

    def test_exact_installed_identifiers(self):
        au.verify_metadata(self.fixture())
        for key, value in [('CFBundleIdentifier', 'another.bundle'), ('CFBundleExecutable', '../elsewhere')]:
            candidate = self.fixture()
            candidate[key] = value
            with self.assertRaises(ValueError):
                au.verify_metadata(candidate)
        for key, value in [('type', 'aumu'), ('subtype', 'Tbs2'), ('manufacturer', 'tmlz'), ('factoryFunction', '')]:
            candidate = self.fixture()
            candidate['AudioComponents'][0][key] = value
            with self.assertRaises(ValueError):
                au.verify_metadata(candidate)

    def test_component_count(self):
        for components in [None, {}, [], [self.fixture()['AudioComponents'][0]] * 2]:
            candidate = self.fixture()
            candidate['AudioComponents'] = copy.deepcopy(components)
            with self.assertRaises(ValueError):
                au.verify_metadata(candidate)

    def test_exact_registry_columns(self):
        self.assertTrue(au.contains_component('other output\n  aumi\tTbs1  Tmlz - Tomas Laurenzo: The Three Body Solution\n'))
        for listing in ['aumi Tbs2 Tmlz - Another', 'aumu Tbs1 Tmlz - Another', 'aumi Tbs1 tmlz - Another',
                        'message mentions aumi Tbs1 Tmlz', 'aumi Tbs1 TmlzExtra', '']:
            self.assertFalse(au.contains_component(listing))

    def test_command_failure_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'failure.txt'
            with self.assertRaises(RuntimeError):
                au.run_logged([sys.executable, '-c', 'print("validator failure"); raise SystemExit(3)'], log)
            self.assertIn('validator failure', log.read_text())
            self.assertIn('EXIT 3', log.read_text())


if __name__ == '__main__':
    unittest.main()
