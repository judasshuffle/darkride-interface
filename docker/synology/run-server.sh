#!/bin/bash
# Adapted from Madders' supervisor; explicit test ports, no unbounded agent forks.
set -eu
cd /opt/darkride
umask 077
python3 ./init-config.py
pids=()
stop() {
    trap '' TERM INT
    if ((${#pids[@]})); then
        kill "${pids[@]}" 2>/dev/null || true
        wait || true
    fi
}
trap 'stop; exit 0' TERM INT
./domain-server --user-config /data/config.json &
pids+=($!)
for assignment in '0 0' '1 1' '3 3' '4 4' '5 5' '6 6'; do
    read -r type offset <<< "$assignment"
    ./assignment-client -t "$type" -a 127.0.0.1 \
        --server-port "$HIFI_DOMAIN_SERVER_PORT" -p "$((AC_PORT_BASE + offset))" &
    pids+=($!)
done
# Scripted agents (type 2) are omitted for the initial LAN test.
set +e
wait -n
status=$?
echo "Server process exited ($status); stopping container" >&2
stop
exit 1
