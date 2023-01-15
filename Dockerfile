FROM debian:bullseye-slim as bind-build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
  && apt-get -y --no-install-recommends install \
  gcc \
  libc6-dev \
  && rm -rf /var/lib/apt/lists/* \
  && mkdir /bind

WORKDIR /bind
COPY bind.c .
RUN gcc -nostartfiles -fpic -shared bind.c -o bind.so -ldl -D_GNU_SOURCE

FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND=noninteractive

RUN groupadd --gid "${PGID:-1000}" valheim \
  && useradd --shell /bin/bash --no-create-home --uid "${PUID:-1000}" --gid "${PUID:-1000}" valheim \
  && apt-get update \
  && apt-get -y dist-upgrade \
  && apt-get -y --no-install-recommends install \
  ca-certificates \
  curl \
  iproute2 \
  jq \
  lib32gcc-s1 \
  libatomic1 \
  libc6 \
  libpulse-dev \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

ARG valheim_install_path=/opt/valheim
ENV VALHEIM_INSTALL_PATH=$valheim_install_path

RUN rm -f /bin/sh \
  && ln -s /bin/bash /bin/sh \
  && install -d -m 0755 -o valheim -g valheim "$VALHEIM_INSTALL_PATH" /opt/steamcmd /config

COPY --from=bind-build /bind/bind.so /usr/local/lib/bind.so

ENV LD_LIBRARY_PATH=/usr/local/lib

USER valheim:valheim

COPY --chown=valheim:valheim valheim-start /usr/local/bin
COPY --chown=valheim:valheim valheim-update /usr/local/bin

RUN curl -L https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz | tar xz -C /opt/steamcmd/ \
  && chmod 755 /opt/steamcmd/steamcmd.sh \
  /opt/steamcmd/linux32/steamcmd \
  /opt/steamcmd/linux32/steamerrorreporter \
  /usr/local/bin/valheim-* \
  && /opt/steamcmd/steamcmd.sh +quit

RUN valheim-update

CMD [ "valheim-start" ]