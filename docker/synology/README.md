# DarkRide 1.1 Synology beta

Status: pipeline reviewed and lightweight checks passed; **not compiled, published, run, or tested with a viewer**.
Work is isolated on `server/synology-1.1`, based on source `77d777ab56`.
Do not migrate Crompton Moor until the acceptance checks below pass.

## Build provenance and resume notes

Reference workflow: https://github.com/judasshuffle/robotdolly-darkride-server/blob/main/.github/workflows/build-linux.yml

Successful reference run: https://github.com/judasshuffle/robotdolly-darkride-server/actions/runs/39387313002

Packaging was copied/adapted from Madders/darkride-interface commit
`685bb3c0fd8b65b5676dd16be2198653a228e1f2` (`darkride-builds`). This is the
inspected reference revision, not a claim that the historical image used exactly that revision.
Keep `ghcr.io/judasshuffle/robotdolly-darkride-server:madders-test` untouched.

The Docker build applies `linux-build.patch` inside its source copy: distro Qt header
discovery, shared-library private Qt headers, and the Overte mirror of the existing
WebRTC archive with its original checksum. No host source/build files are patched.
It retains vcpkg 2021.05.12 and the repository's Wasmtime 49.0.3 integration.
`vcpkg-bootstrap-cstdint.patch` fixes the first CI failure (run 40238623267):
the pinned tool's `strings.h:301` uses `std::uint64_t` without `<cstdint>` under
GCC 13.3.0. The container-only patch adds `-include cstdint` to a copy of
`CXXFLAGS` used only for Linux vcpkg 2021.05.12 bootstrap. Existing flags are
preserved; port/server build environments, the compiler and vcpkg pin are unchanged.
Run `python docker/synology/check-vcpkg-bootstrap.py` to validate patch application
and bootstrap flag isolation without downloading or building dependencies.
This local check does not establish successful GCC 13 compilation; that requires
the next separately approved CI build.
The JavaScript guest is built by the existing `tools/wasm-js/build.sh`, with WASI SDK
34.0 pinned by its GitHub release SHA-256. Packaging requires that guest and checks
runtime shared-library resolution. ICE is not built. Six assignment workers run;
scripted-agent workers (type 2, e.g. bots/persistent assignment scripts) are omitted
for the initial LAN trial. Entity scripts still have their dedicated worker.

## Resume: publish and trigger only after approval

The reviewed destination is `judasshuffle/darkride-interface` (your fork, default
branch `darkride-builds`). `origin` is Caitlyn's upstream: never push there.
The local `fork` remote and branch-specific push remote target your fork only.
The build workflow checks out the dispatch's exact SHA, checks patch applicability,
and only runs in your fork on `server/synology-1.1`. No moving upstream is fetched.

GitHub requires the manual workflow on the default branch. A separate local
`server/synology-dispatch-registration` commit adds **only this workflow YAML** on
top of the fork's `darkride-builds`; it does not merge experimental source. Its job
is skipped on the default branch. Its commit message includes `[skip ci]` to prevent
the existing push-triggered Release workflow from running. Preserve that message;
GitHub skip instructions do not suppress the later manual dispatch. Review its
one-file diff before pushing.
Do not change the fork's default branch or touch the existing release workflow.

Before building, grant `judasshuffle/darkride-interface` **Write** under the existing
GHCR package's Settings -> Manage Actions access. Repository `packages: write`
alone does not grant access to a package owned by another repository. The current
CLI token cannot inspect package settings (it lacks `read:packages`). No PAT is
placed in the image or workflow.

After approval, push both refs without force, then verify registration:

```sh
git push --atomic fork server/synology-1.1:refs/heads/server/synology-1.1 server/synology-dispatch-registration:refs/heads/darkride-builds
gh workflow view synology-beta.yml --repo judasshuffle/darkride-interface
gh workflow run synology-beta.yml --repo judasshuffle/darkride-interface --ref server/synology-1.1
```

If the default branch advances, rebuild the workflow-only registration commit on
its new tip; never force-push. Confirm the remote server branch SHA equals the
reviewed local commit before dispatch. Do not dispatch until explicitly approved.
Use `gh run list --repo judasshuffle/darkride-interface --workflow synology-beta.yml`
and `gh run view RUN_ID --repo judasshuffle/darkride-interface --log-failed` to
inspect actual failures before changing code or scheduling another build.
No local Docker build, Docker Desktop/BIOS changes, NAS operation, or deployment is
authorized by these steps. Deployment still needs a successful image and separate
explicit approval. Keep the Windows viewer and its local edits untouched.

When authorized, manually dispatch on `server/synology-1.1`. There are no push,
schedule, or deployment triggers. The only published tag is:

`ghcr.io/judasshuffle/robotdolly-darkride-server:1.1-beta-<full-source-commit>`

Record the digest printed in the Actions summary and use `image@sha256:...` for
deployment and rollback. Rerunning the same SHA can update its beta tag because apt
repositories and the Ubuntu tag are not snapshot-pinned. This is a source-pinned
recipe, not a bit-for-bit reproducibility claim. Archive the successful image/digest.
BuildKit cache mounts reuse dependencies where the builder retains its cache; hosted
runner cache retention across jobs is not configured. Start with two build jobs;
large dependency compilation may exhaust runner RAM/disk. Stop on such failures.

An optional build on an approved Linux build host (not the NAS or Windows Desktop):

```sh
docker build --platform linux/amd64 -f docker/synology/Dockerfile \
  --build-arg SOURCE_REVISION="$(git rev-parse HEAD)" \
  -t "darkride-server:1.1-beta-$(git rev-parse HEAD)" .
```

## Independent LAN trial

Use a separate Linux host first, then a new Container Manager project if authorized.
Copy `docker-compose.yml` and `.env.example` into a new project directory, copy the
latter to `.env`, and set the verified image digest. Compose does not build images.
Use a new, empty, absolute `DARKRIDE_DATA_DIR`, never any live world's directory.
Create it manually and grant only the chosen `PUID:PGID` write access (default
10001:10001); do not recursively change permissions on existing NAS shares.
The long bind syntax refuses to create a missing host directory automatically.

`/data` contains config.json, entities, ATP assets/mappings, backups and scripts;
HOME and XDG settings/cache/data also live below it. Application logs go to Docker's
rotating log driver. Keep configured entity/asset/script paths under `/data`.
Fresh config disables automatic networking and sets the chosen UDP domain port.
Existing config is never overwritten by the initializer: conflicting port or
networking settings stop startup. Credentials, when set through the UI, are in the
mounted state, not image layers. Treat backups as sensitive.

The container runs non-root, read-only except `/data` and temporary files, drops
capabilities, and uses no Docker socket or privileged mode. Host networking follows
the proven recipe so advertised UDP addresses remain reachable; it also means ports
bind directly on the host. There is no Docker `ports:` filtering. Before starting,
check that all ports are unused and restrict them to the test LAN using the host/DSM
firewall. Do not alter existing server rules or forward router ports. Host mode does
not itself enforce LAN-only access. No ICE or metaverse domain registration is
needed for the direct-address trial; the app may still contact external services.

| Variable | Default | Purpose |
| --- | --- | --- |
| DARKRIDE_IMAGE | required | Verified beta tag/digest |
| DARKRIDE_DATA_DIR | required | New bind-mounted state directory |
| PUID / PGID | 10001 / 10001 | Non-root host file ownership |
| MEMORY_LIMIT | 2g | Initial cap, not a measured requirement |
| HTTP_PORT | 41100 TCP | Admin UI, trusted LAN only |
| HTTPS_PORT | 41101 TCP | Admin TLS, only when certificates configured |
| DOMAIN_PORT | 41102 UDP | Direct viewer connection |
| DTLS_PORT | 41103 UDP | Domain DTLS, if enabled |
| AC_PORT_BASE | 48100 UDP | Audio +0, avatar +1, assets +3, messages +4, entity scripts +5, entities +6 |

The DS423+ has limited memory: leave headroom for DSM and other services. The 2 GiB
container cap is only a trial budget, not evidence that stock NAS RAM is sufficient.
Measure CPU/RAM with a small world and two viewers before increasing load. Build on
GitHub Actions rather than consuming NAS resources. The NAS CPU's compatibility
with the new Wasmtime binary also remains to be exercised.

From the new project directory, after authorization:

```sh
docker compose --env-file .env config
docker compose --env-file .env pull
docker compose --env-file .env up -d
docker compose --env-file .env logs --tail 100 -f
docker compose --env-file .env stop -t 120
docker compose --env-file .env down
```

Restart policy is deliberately `no` during trials. A failed worker stops the whole
container rather than leaving a partially working world. An init process reaps
children; TERM is forwarded and shutdown has 120 seconds for persistence.

## Acceptance checks (not yet performed)

1. Inspect build logs: both server targets and WASM guest complete, codecs packaged,
   no unresolved libraries. Save image digest and SOURCE_REVISION.
2. Check all six workers connect, no crash loops or missing resource errors. Open
   `http://HOST:41100`; set admin credentials before granting broader LAN access.
3. Connect the actual DarkRide 1.1 Windows viewer to `hifi://HOST:41102`. Record its
   exact version/commit and both logs. Protocol is a signature of packet versions
   (`PacketHeaders.cpp`), not the release label. Never disable mismatch checks.
4. With two viewers, test avatars/audio, ATP upload/download, entity edits and an
   entity script. Verify permitted/denied actions match the configured permissions.
5. Stop/recreate the container and verify entities, assets and settings survive.
   Check memory/CPU under load and test a backup restoration to a new directory.
6. Verify Crompton Moor remains untouched and available. Public deployment and
   migration require a separate review; they are not authorized by this recipe.

## Backup and rollback

Stop the beta first (`docker compose stop -t 120`) and confirm all processes exited
cleanly. Back up the **entire beta data directory**, `.env`, compose file, image
digest and logs. Use DSM snapshots or, on Linux, `tar --numeric-owner -czf
NEW-BACKUP.tar.gz -C /path/to/beta-data .` with a new backup filename outside the data
directory. Export the image with `docker image save` if offline rollback is needed.

To roll back: stop this beta, restore the matching backup to a **new empty directory**,
set DARKRIDE_DATA_DIR to that directory and DARKRIDE_IMAGE to its recorded digest,
then start the beta project again. Never start older code against upgraded state
without a matching backup. Never use `down -v`, volume pruning or overwrite a live
world. The existing `madders-test` deployment remains a separate project throughout.

## Lightweight local checks

`python docker/synology/check-config.py` checks YAML structure and initializer
behaviour using temporary state. `git apply --check docker/synology/linux-build.patch`
checks the Linux patch without applying it. `bash -n` checks the two shell scripts.
These do not establish Linux compilation, Compose-engine compatibility, runtime
behaviour, NAS suitability or viewer protocol compatibility.

## Follow-up to Assimp failure (run 40239218615)

The bootstrap include fix passed CI; Draco and zlib built, then Assimp failed.
The original run did not expose Assimp's compiler log, so its precise cause is
unconfirmed. Madders' successful v2026.10.6 source
`f5ce86141be993187959d9311c4175ab346fe667` also adds `-include cstdint` to Linux
vcpkg dependency triplets. `vcpkg-ports-cstdint.patch` adapts that fix, retaining
existing C++ flags and leaving C flags unchanged. It applies only inside the
container, to Linux vcpkg 2021.05.12; no compiler or dependency upgrade is made.

On configure/dependency failure, `show-build-errors.py` prints the first error
context and final 60 lines of the ten newest logs under /vcpkg and /build before
exiting with the original failure status. This exposes dependency diagnostics in
Actions without needing access to the discarded container. A successful Linux
compile still requires a separately approved manual build.
