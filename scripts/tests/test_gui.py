import base64
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch


class ControllerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.state = tempfile.TemporaryDirectory()
        with patch.dict(os.environ, SCOREBOARD_STATE_DIR=cls.state.name, SCOREBOARD_PASSWORD='test-password'):
            spec = importlib.util.spec_from_file_location('controller', Path(__file__).resolve().parents[1] / 'gui.py')
            cls.gui = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.gui)
        cls.gui.app.config['TESTING'] = True

    @classmethod
    def tearDownClass(cls):
        cls.gui.process = None
        cls.state.cleanup()

    def setUp(self):
        self.gui.process = None
        self.client = self.gui.app.test_client()
        self.headers = {
            'Authorization': 'Basic ' + base64.b64encode(b'scoreboard:test-password').decode(),
            'X-CSRF-Token': self.gui.CSRF_TOKEN,
        }

    def post(self, path, data):
        return self.client.post(path, json=data, headers=self.headers)

    def test_auth_and_csrf_required(self):
        self.assertEqual(self.client.get('/').status_code, 401)
        self.assertEqual(self.client.post('/start', json={}, headers={'Authorization': self.headers['Authorization']}).status_code, 403)
        self.assertEqual(self.client.get('/start', headers=self.headers).status_code, 405)
        self.assertEqual(self.client.get('/stop', headers=self.headers).status_code, 405)

    def test_page_renders_csrf_and_safe_city_elements(self):
        response = self.client.get('/', headers=self.headers)
        self.assertEqual(response.status_code, 200)
        self.assertIn(self.gui.CSRF_TOKEN, response.text)
        self.assertIn('document.createTextNode(city', response.text)
        Path(self.state.name, 'rendered.html').write_text(response.text)

    def test_settings_persist_and_validate(self):
        settings = {'fields': {'mlb': True}, 'cities': ["O'Fallon"], 'countdown': ['', '', '', '']}
        self.assertEqual(self.post('/settings', settings).status_code, 200)
        self.assertEqual(self.client.get('/settings', headers=self.headers).json, settings)
        self.assertEqual(__import__('json').loads(self.gui.SETTINGS_FILE.read_text()), settings)
        self.assertEqual(self.post('/settings', {'fields': []}).status_code, 400)

    def test_validation_blocks_bad_commands(self):
        for args in ({}, {'mode': 'weather'}, {'mode': 'rankings'}, {'mode': 'invalid'},
                     {'mode': 'sports', 'MLB': '1', 'conf': ['ACC|--nfl']},
                     {'mode': 'countdown', 'month': '2', 'day': '31', 'hour': '0', 'minute': '0'}):
            with self.subTest(args=args):
                self.assertEqual(self.post('/start', args).status_code, 400)

    def test_start_status_and_no_duplicate_start(self):
        child = Mock(pid=1234)
        child.poll.return_value = None
        child.wait.side_effect = subprocess.TimeoutExpired('display', .3)
        with patch.object(self.gui.subprocess, 'Popen', return_value=child) as launch:
            response = self.post('/start', {'mode': 'sports', 'MLB': '1'})
            self.assertEqual(response.status_code, 200)
            self.assertTrue(response.json['running'])
            self.post('/start', {'mode': 'sports', 'MLB': '1'})
            launch.assert_called_once()
            args, kwargs = launch.call_args
            self.assertTrue(Path(args[0][-2]).is_absolute())
            self.assertEqual(args[0][-1], '--mlb')
            self.assertEqual(kwargs['cwd'], self.gui.ROOT / 'RaspberryPI')
            self.assertTrue(kwargs['start_new_session'])
        self.assertTrue(self.client.get('/status', headers=self.headers).json['running'])
        child.poll.return_value = 1
        self.assertFalse(self.client.get('/status', headers=self.headers).json['running'])

    def test_modes_build_expected_arguments(self):
        cases = [
            ({'mode': 'weather', 'city': ["O'Fallon", 'Chicago']},
             ['--weather_display', '--city', "O'Fallon", '--city', 'Chicago']),
            ({'mode': 'rankings', 'NCAAFRankings': '1', 'NCAABRankings': '1'},
             ['--college-football-rankings', '--mens-college-basketball-rankings']),
            ({'mode': 'countdown', 'month': '12', 'day': '25', 'hour': '0', 'minute': '0', 'event': 'Christmas'},
             ['--countdown_display', '--event', 'Christmas', '--month', '12', '--day', '25', '--hour', '0', '--minute', '0']),
        ]
        for data, expected in cases:
            with self.subTest(mode=data['mode']):
                self.gui.process = None
                child = Mock()
                child.wait.side_effect = subprocess.TimeoutExpired('display', .3)
                with patch.object(self.gui.subprocess, 'Popen', return_value=child) as launch:
                    self.assertEqual(self.post('/start', data).status_code, 200)
                    command = launch.call_args.args[0]
                    executable = command.index(str(self.gui.ROOT / 'RaspberryPI' / 'espn_jsonc'))
                    self.assertEqual(command[executable + 1:], expected)

    def test_start_errors_are_reported(self):
        with patch.object(self.gui.subprocess, 'Popen', side_effect=FileNotFoundError('missing')):
            self.assertEqual(self.post('/start', {'MLB': '1'}).status_code, 500)
        child = Mock()
        child.wait.return_value = 1
        with patch.object(self.gui.subprocess, 'Popen', return_value=child):
            self.assertEqual(self.post('/start', {'MLB': '1'}).status_code, 500)

    def test_stop_waits_and_escalates(self):
        child = Mock(pid=1234)
        child.poll.return_value = None
        child.wait.side_effect = [subprocess.TimeoutExpired('display', 5), 0]
        self.gui.process = child
        with patch.object(self.gui.os, 'killpg') as kill:
            self.assertEqual(self.post('/stop', {}).json, {'running': False})
            self.assertEqual(kill.call_args_list[0].args, (1234, signal.SIGTERM))
            self.assertEqual(kill.call_args_list[1].args, (1234, signal.SIGKILL))
        self.assertIsNone(self.gui.process)
        self.assertEqual(self.post('/stop', {}).json, {'running': False})


if __name__ == '__main__':
    unittest.main()
