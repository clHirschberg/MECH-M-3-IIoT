import json
from contextlib import closing
import sqlite3
import tempfile
import time
import unittest
from pathlib import Path
import collector
from collector import connect_db, normalize, parse_status, store
from datetime import datetime, timezone


def iso(timestamp, fmt='%Y-%m-%dT%H:%M:%SZ'):
    return datetime.fromtimestamp(timestamp, timezone.utc).strftime(fmt)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = str(Path(self.tmp.name) / 'test.sqlite')
        self.db = connect_db(self.path)

    def tearDown(self):
        self.db.close()
        self.tmp.cleanup()

    def test_existing_esp_and_specification(self):
        store(self.db, 'iiot/group/a/sensor/temperature/state', json.dumps(
            dict(sensorId='esp', timestamp=1234, temperature=22.5, humidity=50)).encode(), received=100)
        store(self.db, 'iiot/group/b/sensor/temperature/state', json.dumps(
            dict(sensor_id='spec', timestamp=iso(time.time()), value=21, unit='°C')).encode())
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM readings').fetchone()[0], 3)
        self.assertEqual(self.db.execute('SELECT time,time_source FROM readings ORDER BY id LIMIT 1').fetchone(), (100, 'received'))
        self.assertEqual(self.db.execute('SELECT time_source FROM readings WHERE sensor_id="spec"').fetchone()[0], 'device')

    def test_plain_status_and_metadata_are_archived(self):
        for payload in (b'offline', b'"ok"', b'{"sensorId":"esp","unit":"C"}'):
            store(self.db, 'iiot/group/a/status', payload)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 3)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages WHERE parse_error IS NOT NULL').fetchone()[0], 0)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM readings').fetchone()[0], 0)

    def test_bad_payload_is_preserved(self):
        for payload in [b'\xff', b'not json', b'{"value":NaN}', b'{"value":true}']:
            store(self.db, 'iiot/group/a/sensor/temperature/state', payload)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages WHERE parse_error IS NOT NULL').fetchone()[0], 4)
        self.assertEqual(self.db.execute('SELECT payload FROM messages LIMIT 1').fetchone()[0], b'\xff')

    def test_readonly_concurrent_reader_and_restore(self):
        with sqlite3.connect(f'file:{self.path}?mode=ro', uri=True) as reader:
            for i in range(1000):
                store(self.db, 'iiot/group/a/sensor/temperature/state', b'{"temperature":22}')
                self.assertEqual(reader.execute('SELECT COUNT(*) FROM messages').fetchone()[0], i + 1)
        with sqlite3.connect(str(Path(self.tmp.name)/'backup.sqlite')) as backup:
            self.db.backup(backup)
            self.assertEqual(backup.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
            self.assertEqual(backup.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 1000)

    def test_backup_restore_and_reject_invalid_source(self):
        original_path = collector.DB
        collector.DB = self.path
        try:
            store(self.db, 'iiot/group/a/sensor/temperature/state', b'{"temperature":22}')
            saved = str(Path(self.tmp.name) / 'saved.sqlite')
            collector.backup(saved)
            store(self.db, 'iiot/group/a/sensor/temperature/state', b'{"temperature":23}')
            self.db.close()
            collector.restore(saved)
            self.db = connect_db(self.path)
            self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 1)
            invalid = str(Path(self.tmp.name) / 'invalid.sqlite')
            with sqlite3.connect(invalid) as db:
                db.execute('CREATE TABLE unrelated(value)')
            with self.assertRaises(RuntimeError):
                collector.restore(invalid)
            self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 1)
        finally:
            collector.DB = original_path


class StatusTests(unittest.TestCase):
    """ISSUES #1: Status aus iiot/group/+/status in der Tabelle devices."""

    def setUp(self):
        self.db = connect_db(':memory:')

    def tearDown(self):
        self.db.close()

    def device(self, group):
        self.db.row_factory = sqlite3.Row
        row = self.db.execute('SELECT * FROM devices WHERE group_name=?', (group,)).fetchone()
        self.db.row_factory = None
        return dict(row) if row else None

    def test_parse_status_variants(self):
        cases = {
            b'ok': 'ok', b' OFFLINE\n': 'offline', b'"error"': 'error',
            b'{"status": "ok", "sensor_id": "x"}': 'ok', b'{"status": "Offline"}': 'offline',
            b'{"sensorId": "esp", "unit": "C"}': 'unknown', b'{"status": 5}': 'unknown',
            b'[1, 2]': 'unknown', b'kaputt': 'unknown', b'\xff': 'unknown', b'': None, b'  ': None,
        }
        for payload, expected in cases.items():
            with self.subTest(payload=payload):
                self.assertEqual(parse_status(payload), expected)

    def test_status_before_first_reading(self):
        store(self.db, 'iiot/group/A-b/status', b'{"status":"ok"}', received=100)
        self.assertEqual(self.device('a-b'), dict(group_name='a-b', status='ok', status_updated_at=100,
                                                  last_seen=None, last_value=None, unit=None))

    def test_status_uses_plausible_device_timestamp(self):
        now = time.time()
        store(self.db, 'iiot/group/a/status', json.dumps({'status': 'error', 'timestamp': iso(now - 60)}).encode(), received=now)
        self.assertAlmostEqual(self.device('a')['status_updated_at'], int(now - 60), delta=1)
        store(self.db, 'iiot/group/a/status', b'{"status":"ok","timestamp":"1970-01-01T00:00:12Z"}', received=now)
        self.assertEqual(self.device('a')['status_updated_at'], now)

    def test_reading_and_status_do_not_overwrite_each_other(self):
        store(self.db, 'iiot/group/a/status', b'{"status":"offline"}', received=100)
        store(self.db, 'iiot/group/a/sensor/temperature/state', b'{"value":21.5,"unit":"\xc2\xb0C"}', received=200)
        store(self.db, 'iiot/group/a/sensor/humidity/state', b'{"value":40,"unit":"%"}', received=250)
        device = self.device('a')
        self.assertEqual((device['status'], device['status_updated_at']), ('offline', 100))
        self.assertEqual((device['last_seen'], device['last_value'], device['unit']), (250, 21.5, '°C'))
        store(self.db, 'iiot/group/a/status', b'{"status":"ok"}', received=300)
        self.assertEqual(self.device('a')['last_value'], 21.5)

    def test_empty_status_payload_is_ignored(self):
        store(self.db, 'iiot/group/a/status', b'{"status":"ok"}', received=100)
        store(self.db, 'iiot/group/a/status', b'', received=200)
        self.assertEqual(self.device('a')['status'], 'ok')
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 2)

    def test_status_topic_never_creates_readings(self):
        store(self.db, 'iiot/group/a/status', b'{"status":"ok","value":5}')
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM readings').fetchone()[0], 0)


class NormalizeTests(unittest.TestCase):
    """ISSUES #3, #4, #5: Gruppenname, Zeitstempel und Toleranz bei value/unit."""

    TOPIC = 'iiot/group/Kloepfer-Brose/sensor/temperature/state'

    def parse(self, received=None, topic=TOPIC, **data):
        received = time.time() if received is None else received
        return normalize(topic, json.dumps(data).encode(), received)

    def test_group_is_lowercase(self):
        self.assertEqual(self.parse(value=20)[0][2], 'kloepfer-brose')

    def test_empty_group_is_rejected(self):
        with self.assertRaises(ValueError):
            self.parse(topic='iiot/group//sensor/temperature/state', value=20)
        db = connect_db(':memory:')
        store(db, 'iiot/group//sensor/temperature/state', b'{"value":20}')
        self.assertEqual(db.execute('SELECT COUNT(*) FROM readings').fetchone()[0], 0)
        self.assertIsNotNone(db.execute('SELECT parse_error FROM messages').fetchone()[0])

    def test_timestamps(self):
        now = 1_800_000_000.0
        cases = [
            (iso(now - 10), now - 10, 'device'),
            (iso(now - 10, '%Y-%m-%dT%H:%M:%S'), now - 10, 'device'),  # ohne Zone = UTC
            (datetime.fromtimestamp(now - 10, timezone.utc).astimezone().isoformat(), now - 10, 'device'),
            (iso(now - 301), now, 'received'),
            ('1970-01-01T00:00:12Z', now, 'received'),
            (1234, now, 'received'),
            ('gestern', now, 'received'),
        ]
        for stamp, expected, source in cases:
            with self.subTest(stamp=stamp):
                row = self.parse(received=now, value=20, timestamp=stamp)[0]
                self.assertEqual((row[0], row[1]), (expected, source))
        row = self.parse(received=now, value=20)[0]
        self.assertEqual((row[0], row[1]), (now, 'received'))

    def test_value_and_unit_tolerance(self):
        self.assertEqual(self.parse(value='21.5', unit='°C')[0][5:], (21.5, '°C'))
        self.assertEqual(self.parse(value=21.5)[0][5:], (21.5, '°C'))
        self.assertEqual(self.parse(value=294.65, unit='K')[0][5:], (21.5, '°C'))
        for bad in (True, 'warm', None, [1]):
            with self.subTest(value=bad), self.assertRaises((ValueError, TypeError)):
                self.parse(value=bad)

    def test_sensor_id_fallback(self):
        self.assertEqual(self.parse(value=20)[0][3], '-')
        self.assertEqual(self.parse(value=20, sensor_id='esp-1')[0][3], 'esp-1')


class MigrationTests(unittest.TestCase):
    def test_v1_database_is_migrated(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = str(Path(tmp) / 'v1.sqlite')
            with closing(sqlite3.connect(path)) as old:
                old.executescript('''
                    CREATE TABLE messages (id INTEGER PRIMARY KEY, received_at REAL NOT NULL,
                        topic TEXT NOT NULL, payload BLOB NOT NULL, qos INTEGER NOT NULL,
                        retained INTEGER NOT NULL, parse_error TEXT);
                    CREATE TABLE readings (id INTEGER PRIMARY KEY, message_id INTEGER NOT NULL,
                        time REAL NOT NULL, time_source TEXT NOT NULL, group_name TEXT NOT NULL,
                        sensor_id TEXT NOT NULL, metric TEXT NOT NULL, value REAL NOT NULL, unit TEXT NOT NULL);
                    INSERT INTO messages VALUES (1, 100, 'iiot/group/Ab/sensor/temperature/state', x'00', 0, 0, NULL);
                    INSERT INTO readings VALUES (1, 1, 100, 'received', 'Ab', 's', 'temperature', 20, '°C');
                    INSERT INTO messages VALUES (2, 150, 'iiot/group/ab/status', CAST('{"status":"ok"}' AS BLOB), 0, 1, NULL);
                    INSERT INTO messages VALUES (3, 160, 'iiot/group/cd/status', CAST('offline' AS BLOB), 0, 1, NULL);
                    PRAGMA user_version=1;
                ''')
            db = connect_db(path)
            self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0], 2)
            self.assertEqual(db.execute('SELECT DISTINCT group_name FROM readings').fetchall(), [('ab',)])
            self.assertEqual(db.execute('SELECT * FROM devices ORDER BY group_name').fetchall(),
                             [('ab', 'ok', 150, 100, 20, '°C'), ('cd', 'offline', 160, None, None, None)])
            db.close()
            connect_db(path).close()  # zweites Öffnen migriert nicht erneut


if __name__ == '__main__':
    unittest.main()
