"""pytest-Tests der HTTP-API des Mikrocontrollers (docs/iot-specs/openapi.yaml 1.2.0).

Ausführen (Gerät im selben WLAN):

    DEVICE_URL=http://<ip-des-geraets> pdm run pytest tests -v
"""

import time
from datetime import datetime

import pytest

from conftest import TIMEOUT


def post_config(http, base_url, **kwargs):
    return http.post(f'{base_url}/config', timeout=TIMEOUT, **kwargs)


def assert_error(response, status):
    assert response.status_code == status, response.text
    assert response.headers['Content-Type'].startswith('application/json')
    assert isinstance(response.json()['error'], str)


# ----- GET /config -----

def test_get_config(http, base_url):
    response = http.get(f'{base_url}/config', timeout=TIMEOUT)
    assert response.status_code == 200
    assert response.headers['Content-Type'].startswith('application/json')
    config = response.json()
    assert isinstance(config['device_id'], str) and config['device_id']
    assert isinstance(config['reading_interval_seconds'], int)
    assert config['reading_interval_seconds'] >= 2


# ----- POST /config -----

@pytest.mark.parametrize('interval', [2, 17, 86400])
def test_post_config_roundtrip(http, base_url, interval):
    response = post_config(http, base_url, json={'reading_interval_seconds': interval})
    assert response.status_code == 200, response.text
    assert response.json()['reading_interval_seconds'] == interval
    config = http.get(f'{base_url}/config', timeout=TIMEOUT).json()
    assert config['reading_interval_seconds'] == interval


def test_post_config_ignores_unknown_fields(http, base_url):
    response = post_config(http, base_url, json={'reading_interval_seconds': 30, 'foo': 'bar'})
    assert response.status_code == 200, response.text
    assert response.json()['reading_interval_seconds'] == 30


def test_post_config_cannot_change_device_id(http, base_url):
    before = http.get(f'{base_url}/config', timeout=TIMEOUT).json()
    response = post_config(http, base_url, json={'reading_interval_seconds': 30, 'device_id': 'hacked'})
    assert_error(response, 400)
    after = http.get(f'{base_url}/config', timeout=TIMEOUT).json()
    assert after == before


@pytest.mark.parametrize('body', [
    {},
    {'reading_interval_seconds': 1},
    {'reading_interval_seconds': 0},
    {'reading_interval_seconds': -5},
    {'reading_interval_seconds': 86401},
    {'reading_interval_seconds': 'zehn'},
    {'reading_interval_seconds': 2.5},
    {'reading_interval_seconds': None},
    {'reading_interval_seconds': True},
    [10],
    'reading_interval_seconds',
])
def test_post_config_rejects_invalid_values(http, base_url, body):
    before = http.get(f'{base_url}/config', timeout=TIMEOUT).json()
    assert_error(post_config(http, base_url, json=body), 400)
    assert http.get(f'{base_url}/config', timeout=TIMEOUT).json() == before


@pytest.mark.parametrize('data', ['{"reading_interval_seconds": ', 'kein json', ''])
def test_post_config_rejects_broken_json(http, base_url, data):
    response = post_config(http, base_url, data=data, headers={'Content-Type': 'application/json'})
    assert_error(response, 400)


# ----- GET /status -----

def test_get_status(http, base_url):
    response = http.get(f'{base_url}/status', timeout=TIMEOUT)
    assert response.status_code == 200
    assert response.headers['Content-Type'].startswith('application/json')
    status = response.json()
    config = http.get(f'{base_url}/config', timeout=TIMEOUT).json()
    assert status['device_id'] == config['device_id']
    assert isinstance(status['uptime_seconds'], int) and status['uptime_seconds'] >= 0
    assert isinstance(status['wifi_connected'], bool)
    assert status['wifi_connected'] is True  # sonst wäre das Gerät nicht erreichbar
    assert isinstance(status['mqtt_connected'], bool)
    assert 'last_telemetry' in status
    assert 'last_humidity_percent' in status

    telemetry = status['last_telemetry']
    if telemetry is not None:
        assert isinstance(telemetry['value'], (int, float))
        assert telemetry['unit'] == '°C'
        assert telemetry['timestamp'].endswith('Z')
        datetime.fromisoformat(telemetry['timestamp'].replace('Z', '+00:00'))
    humidity = status['last_humidity_percent']
    assert humidity is None or 0 <= humidity <= 100


def test_uptime_increases(http, base_url):
    first = http.get(f'{base_url}/status', timeout=TIMEOUT).json()['uptime_seconds']
    time.sleep(2)
    second = http.get(f'{base_url}/status', timeout=TIMEOUT).json()['uptime_seconds']
    assert second > first


def test_new_interval_is_used(http, base_url):
    """Nach POST /config misst und sendet das Gerät im neuen Intervall."""
    status = http.get(f'{base_url}/status', timeout=TIMEOUT).json()
    if not status['mqtt_connected']:
        pytest.skip('Gerät ist nicht mit dem MQTT-Broker verbunden')

    assert post_config(http, base_url, json={'reading_interval_seconds': 2}).status_code == 200
    seen = set()
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline and len(seen) < 3:
        telemetry = http.get(f'{base_url}/status', timeout=TIMEOUT).json()['last_telemetry']
        if telemetry is not None:
            seen.add(telemetry['timestamp'])
        time.sleep(1)
    # Bei 2 s Intervall kommen in 12 s mindestens drei neue Messungen
    assert len(seen) >= 3, f'nur {len(seen)} verschiedene Messzeitpunkte: {sorted(seen)}'


# ----- Fehlerbehandlung -----

@pytest.mark.parametrize('path', ['/unbekannt', '/config/extra', '/api/status'])
def test_unknown_path_returns_404(http, base_url, path):
    assert_error(http.get(f'{base_url}{path}', timeout=TIMEOUT), 404)


@pytest.mark.parametrize('method, path', [
    ('PUT', '/config'),
    ('DELETE', '/config'),
    ('PATCH', '/config'),
    ('POST', '/status'),
    ('DELETE', '/status'),
])
def test_wrong_method_returns_405(http, base_url, method, path):
    assert_error(http.request(method, f'{base_url}{path}', timeout=TIMEOUT), 405)


def test_device_survives_error_requests(http, base_url):
    """Nach den Fehlerfällen antwortet das Gerät weiterhin normal (kein Absturz)."""
    for _ in range(3):
        post_config(http, base_url, data='{{{', headers={'Content-Type': 'application/json'})
        http.get(f'{base_url}/gibt-es-nicht', timeout=TIMEOUT)
    response = http.get(f'{base_url}/status', timeout=TIMEOUT)
    assert response.status_code == 200
