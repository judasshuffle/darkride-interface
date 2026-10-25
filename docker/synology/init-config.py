"""Create test configuration once; refuse conflicting existing state."""
import json
import os
from pathlib import Path


def main():
    names = ("HIFI_DOMAIN_SERVER_PORT", "HIFI_DOMAIN_SERVER_HTTP_PORT",
             "HIFI_DOMAIN_SERVER_HTTPS_PORT", "HIFI_DOMAIN_SERVER_DTLS_PORT", "AC_PORT_BASE")
    ports = {name: int(os.environ[name]) for name in names}
    all_ports = list(ports.values())[:-1] + [ports["AC_PORT_BASE"] + n for n in (0, 1, 3, 4, 5, 6)]
    if any(p < 1024 or p > 65535 for p in all_ports) or len(set(all_ports)) != len(all_ports):
        raise SystemExit("Ports must be distinct and within 1024..65535")
    root = Path(os.environ["TIVOLI_APP_DATA_DIR"])
    for name in ("home", "config", "cache", "share"):
        (root / name).mkdir(parents=True, exist_ok=True)
    path = root / "config.json"
    config = {"metaverse": {"local_port": ports["HIFI_DOMAIN_SERVER_PORT"],
                            "automatic_networking": "disabled"}}
    try:
        with path.open("x", encoding="utf-8") as stream:
            json.dump(config, stream, indent=2)
    except FileExistsError:
        existing = json.loads(path.read_text(encoding="utf-8"))
        settings = existing.get("metaverse", {})
        if settings.get("local_port") != config["metaverse"]["local_port"]:
            raise SystemExit("Existing config.json port differs from .env; reconcile manually while stopped")
        if settings.get("automatic_networking") != "disabled":
            raise SystemExit("This LAN beta requires automatic_networking=disabled")


if __name__ == "__main__":
    main()
