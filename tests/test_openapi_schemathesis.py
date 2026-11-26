"""Property-based Tests mit Schemathesis gegen docs/iot-specs/openapi.yaml.

Schemathesis erzeugt aus der Spezifikation gültige und ungültige Anfragen und
prüft, dass die Antworten zur Spezifikation passen (Statuscodes, Content-Type,
Response-Schema, keine Serverfehler, ungültige Daten werden abgelehnt).

    DEVICE_URL=http://<ip-des-geraets> pdm run pytest tests/test_openapi_schemathesis.py -v

Alternativ über die Kommandozeile:

    pdm run schemathesis run docs/iot-specs/openapi.yaml --url http://<ip-des-geraets>
"""

from pathlib import Path

import schemathesis
from hypothesis import HealthCheck, settings

from conftest import TIMEOUT

SPEC = Path(__file__).resolve().parent.parent / 'docs' / 'iot-specs' / 'openapi.yaml'
schema = schemathesis.openapi.from_path(str(SPEC))


@schema.parametrize()
@settings(
    max_examples=25,  # Der ESP32 ist langsam, 25 Fälle pro Operation reichen
    deadline=None,
    suppress_health_check=list(HealthCheck),
)
def test_api_conforms_to_openapi(case, base_url):
    case.call_and_validate(base_url=base_url, timeout=TIMEOUT)
