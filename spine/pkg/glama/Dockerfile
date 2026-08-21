# Glama MCP directory (glama.ai) check image — NOT required to run the tool.
#
# logan-spine-mcp needs NO Docker to run. This image exists only so Glama
# can build a sandbox, launch the stdio MCP server, and run its introspection
# checks, which power the directory's score badge. The same image is exercised
# locally and in CI by pkg/glama/verify.sh to guard against drift.
#
# We pull the "-portable" release asset, which is fully statically linked
# (gcc -static) — unlike the standard asset, which dynamically links glibc/
# libstdc++ and would fail on an older base. Because it's static, the runtime
# base image is arbitrary. TARGETARCH is amd64 / arm64, matching the asset names.

FROM debian:bookworm-slim AS fetch
ARG TARGETARCH
RUN apt-get update \
 && apt-get install -y --no-install-recommends curl ca-certificates \
 && curl -fsSL "https://github.com/DeusData/logan-spine-mcp/releases/latest/download/logan-spine-mcp-linux-${TARGETARCH}-portable.tar.gz" \
      | tar -xz -C /tmp logan-spine-mcp LICENSE THIRD_PARTY_NOTICES.md \
 && chmod +x /tmp/logan-spine-mcp \
 && rm -rf /var/lib/apt/lists/*

FROM debian:bookworm-slim
COPY --from=fetch /tmp/logan-spine-mcp /usr/local/bin/logan-spine-mcp
COPY --from=fetch /tmp/LICENSE /tmp/THIRD_PARTY_NOTICES.md /usr/share/doc/logan-spine-mcp/
ENV LSM_CACHE_DIR=/tmp/lsm
ENTRYPOINT ["logan-spine-mcp"]
