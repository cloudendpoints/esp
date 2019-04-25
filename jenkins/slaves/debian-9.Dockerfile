# Stored at gcr.io/endpoints-jenkins/debian-9-slave:latest
FROM debian:stretch-backports

# Bucket used to store already built binaries
ARG TOOLS_BUCKET

# Installing necessary packages
RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq \
    && apt-get install -qqy git iptables procps sudo xz-utils \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Installing Tools
ADD script /tmp/esp_tmp/script
RUN chmod +x /tmp/esp_tmp/script/linux-install-software
RUN /tmp/esp_tmp/script/linux-install-software \
      -p "debian-9" \
      -b "${TOOLS_BUCKET}" \
    && rm -rf /tmp/esp_tmp

ENV PATH /usr/lib/google-cloud-sdk/bin:${PATH}

ADD jenkins/slaves/entrypoint /usr/local/bin/entrypoint
RUN chmod +rx /usr/local/bin/entrypoint

ENTRYPOINT ["/usr/local/bin/entrypoint"]
