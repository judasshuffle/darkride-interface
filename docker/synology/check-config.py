"""Lightweight checks only; requires existing PyYAML, no Docker/builds/downloads."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
workflow = yaml.load((ROOT / '.github/workflows/synology-beta.yml').read_text(), Loader=yaml.BaseLoader)
assert set(workflow['on']) == {'workflow_dispatch'}
assert workflow['jobs']['build']['if'] == "github.repository == 'judasshuffle/darkride-interface' && github.ref == 'refs/heads/server/synology-1.1'"
steps = workflow['jobs']['build']['steps']
publish = next(s for s in steps if s.get('uses', '').startswith('docker/build-push-action'))
assert publish['with']['tags'] == '${{ steps.revision.outputs.image }}'
revision = next(s for s in steps if s.get('id') == 'revision')['run']
assert ':1.1-beta-' in revision and ':latest' not in revision and ':madders-test' not in revision
compose = yaml.safe_load((HERE / 'docker-compose.yml').read_text())
service = compose['services']['beta']
assert service['read_only'] and service['cap_drop'] == ['ALL']
assert service['volumes'][0]['bind']['create_host_path'] is False
assert 'build' not in service and 'privileged' not in service

with tempfile.TemporaryDirectory() as directory:
    env = dict(os.environ, TIVOLI_APP_DATA_DIR=directory,
               HIFI_DOMAIN_SERVER_PORT='41102', HIFI_DOMAIN_SERVER_HTTP_PORT='41100',
               HIFI_DOMAIN_SERVER_HTTPS_PORT='41101', HIFI_DOMAIN_SERVER_DTLS_PORT='41103',
               AC_PORT_BASE='48100')
    def run(ok=True):
        result = subprocess.run([sys.executable, str(HERE / 'init-config.py')], env=env,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stderr
    run()
    config = Path(directory) / 'config.json'
    data = json.loads(config.read_text())
    data['test-preserve'] = 'unchanged'
    config.write_text(json.dumps(data))
    before = config.read_bytes()
    run()
    assert config.read_bytes() == before
    env['HIFI_DOMAIN_SERVER_PORT'] = '42102'
    run(False)
    assert config.read_bytes() == before
    env['HIFI_DOMAIN_SERVER_PORT'] = '41100'
    run(False)
    env['HIFI_DOMAIN_SERVER_PORT'] = '41102'
    data['metaverse']['automatic_networking'] = 'full'
    config.write_text(json.dumps(data))
    run(False)
print('PASS: YAML safeguards; initial config; repeat preservation; conflict/duplicate-port/networking rejection')
