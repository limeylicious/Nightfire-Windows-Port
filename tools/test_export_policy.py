"""Boundary tests for the export gate; never reads or copies game files."""
import json
import sys
import unittest

sys.dont_write_bytecode = True
from export_policy import check_text, read_allowlist, relative_name


class ExportPolicyTests(unittest.TestCase):
    def test_reject_unsafe_paths(self):
        for name in ('../secret', '/absolute', 'a/../b', 'a\\b', 'a//b', 'a:stream', './a'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                relative_name(name)

    def test_reject_game_data_even_under_source_name(self):
        for data in (b'MZpayload', b'XBEHpayload', b'BMpayload', b'PK\x03\x04payload', b'abc\0def'):
            with self.subTest(data=data), self.assertRaises(ValueError):
                check_text('runtime/test.c', data)

    def test_reject_excluded_text_tables(self):
        for name in ('driving_font_plan348.h', 'driving_sprite_contract396.h',
                     'driving_native_effects350.h', 'driving_native_remaining352.h',
                     'driving_contract999.h', 'recomp_manual.c',
                     'DRIVING_NATIVE_EFFECTS350.H', 'Driving_Contract999.h'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                check_text('runtime/' + name, b'/* text is not enough */')

    def test_reject_private_markers(self):
        doubled_separator = bytes([92]) * 2
        cases = [b'C:' + b'/Users/' + b'example/private',
                 b'C:' + doubled_separator + b'Users' + doubled_separator + b'example',
                 b'ghp_' + b'a' * 30, b'sk-' + b'z' * 32,
                 b'-----BEGIN ' + b'PRIVATE KEY-----']
        for data in cases:
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                check_text('runtime/test.c', data)

    def test_allow_portable_source_and_launchers(self):
        check_text('nightfire-driving/build-windows.cmd', b'@echo off\n')
        check_text('experiments/driving-510/runtime/helper.c', b'int helper(void) { return 1; }\n')

    def test_reject_redirected_and_duplicate_allowlist(self):
        row = dict(source='nightfire-driving/runtime/helper.c',
                   path='nightfire-driving/runtime/helper.c', group='normal-source')
        def data(rows):
            return json.dumps(dict(schema='reviewed-source-export-v2', files=rows)).encode()
        with self.assertRaises(ValueError):
            read_allowlist(data([row, row]))
        row['source'] = 'game_files/default.xbe'
        with self.assertRaises(ValueError):
            read_allowlist(data([row]))

    def test_lean_mapping(self):
        def data(row):
            return json.dumps(dict(schema='reviewed-source-export-v2', files=[row])).encode()
        row = dict(source='nightfire-driving-lean/runtime/lean/lean_gpu.c',
                   path='experiments/driving-lean/runtime/lean/lean_gpu.c', group='driving-lean')
        read_allowlist(data(row))
        for source, path in (('nightfire-driving-lean/runtime/lean/lean_gpu.c', 'experiments/driving-lean/runtime/other.c'),
                             ('nightfire-driving-lean/src/recomp/gen/recomp_0007.c', 'experiments/driving-lean/src/recomp/gen/recomp_0007.c'),
                             ('nightfire-driving/runtime/kernel_bridge.c', 'experiments/driving-lean/runtime/kernel_bridge.c')):
            with self.subTest(source=source), self.assertRaises(ValueError):
                read_allowlist(data(dict(row, source=source, path=path)))

    def test_native_mapping(self):
        def data(row):
            return json.dumps(dict(schema='reviewed-source-export-v2', files=[row])).encode()
        for source, path, group in (
                ('nightfire-port-native/runtime/pcg_menu.c', 'native/action/runtime/pcg_menu.c', 'native-action'),
                ('nightfire-driving-native/runtime/lean/lean_d3d.c', 'native/driving/runtime/lean/lean_d3d.c', 'native-driving'),
                ('native-driving/play-nightfire-native.cmd', 'native/launchers/play-nightfire-native.cmd', 'native-launch')):
            read_allowlist(data(dict(source=source, path=path, group=group)))
        for source, path, group in (
                ('nightfire-port-native/runtime/pcg_menu.c', 'native/driving/runtime/pcg_menu.c', 'native-action'),
                ('nightfire-port-native/src/recomp/gen/recomp_0001.c', 'native/action/src/recomp/gen/recomp_0001.c', 'native-action'),
                ('nightfire-driving-native/recovery/old/lean_d3d.c', 'native/driving/recovery/old/lean_d3d.c', 'native-driving'),
                ('native-driving/ingame-menu/tools/edl_helper.dll', 'native/launchers/ingame-menu/tools/edl_helper.dll', 'native-launch'),
                ('nightfire-port/runtime/fb_present.c', 'native/action/runtime/fb_present.c', 'native-action')):
            with self.subTest(source=source), self.assertRaises(ValueError):
                read_allowlist(data(dict(source=source, path=path, group=group)))


    def test_release_mapping(self):
        def data(row):
            return json.dumps(dict(schema='reviewed-source-export-v2', files=[row])).encode()
        read_allowlist(data(dict(source='releases/Nightfire-PC-Alpha/engine/setup.py',
                                 path='release/alpha/engine/setup.py', group='release-alpha')))
        for source, path in (
                ('releases/Nightfire-PC-Alpha/engine/setup.py', 'release/alpha/setup.py'),
                ('releases/Nightfire-Preview/Nightfire.cmd', 'release/alpha/Nightfire.cmd'),
                ('releases/Nightfire-PC-Alpha/engine/action/nightfire_native.exe', 'release/alpha/engine/action/nightfire_native.exe'),
                ('releases/Nightfire-PC-Alpha/engine/action/game_files/default.xbe', 'release/alpha/engine/action/game_files/default.xbe'),
                ('releases/Nightfire-PC-Alpha/engine/action/sessions/a/crash.txt', 'release/alpha/engine/action/sessions/a/crash.txt')):
            with self.subTest(source=source), self.assertRaises(ValueError):
                read_allowlist(data(dict(source=source, path=path, group='release-alpha')))


if __name__ == '__main__':
    unittest.main()
