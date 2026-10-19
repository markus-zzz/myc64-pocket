#!/bin/sh

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P) || exit 1

exec podman run --rm \
  --network=none \
  --cap-drop=ALL \
  --security-opt=no-new-privileges \
  --cpus=4 \
  --memory=8g \
  --pids-limit=256 \
  --read-only \
  --tmpfs /tmp:rw,size=1g \
  --tmpfs /root:rw,size=64m \
  --mount "type=bind,source=$ROOT,target=$ROOT" \
  --workdir "$ROOT" \
  --entrypoint /opt/intelFPGA/quartus/bin/quartus_sh \
  docker.io/patryk4815/quartus@sha256:561d8b1fa5386a21e4d5bf8d16922fcefa086099d5403814da5a57805d858bad \
  "$@"
