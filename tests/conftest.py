"""Gemeinsame Fixtures für die HTTP-API-Tests des Mikrocontrollers.

Die Tests laufen gegen ein echtes Gerät im selben WLAN. Die Adresse kommt aus
der Umgebungsvariable DEVICE_URL, z.B. ``DEVICE_URL=http://192.168.0.100``.
Ohne DEVICE_URL werden alle Tests übersprungen.
"""

import os

import pytest
import requests

TIMEOUT = 5  # Sekunden; der ESP32-Webserver bearbeitet Anfragen nacheinander


def device_url():
    return os.environ.get('DEVICE_URL', '').rstrip('/')


@pytest.fixture(scope='session')
def base_url():
    url = device_url()
    if not url:
        pytest.skip('DEVICE_URL nicht gesetzt, z.B. DEVICE_URL=http://192.168.0.100')
    return url


@pytest.fixture(scope='session')
def http():
    with requests.Session() as session:
        yield session


@pytest.fixture(scope='session', autouse=True)
def restore_config():
    """Merkt sich das Sendeintervall vor den Tests und stellt es danach wieder her.

    POST-Tests und Schemathesis ändern die Gerätekonfiguration.
    """
    url = device_url()
    if not url:
        yield
        return
    original = requests.get(f'{url}/config', timeout=TIMEOUT).json()
    yield
    requests.post(
        f'{url}/config',
        json={'reading_interval_seconds': original['reading_interval_seconds']},
        timeout=TIMEOUT,
    )
