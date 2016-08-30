# ESP Proxy that forwards requests into the application server.
FROM debian:jessie

# Copy the Endpoints runtime Debian package.
ADD endpoints-runtime.deb endpoints-runtime.deb

# Install dependencies
RUN apt-get update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
      apt-utils adduser curl wget \
      python python-pip python-oauth2client python-mako python-urllib3 \
      libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_* && \
    dpkg -i /endpoints-runtime.deb && rm /endpoints-runtime.deb

# Upgrade oauth2client to a version that supports service accounts
RUN pip install --upgrade oauth2client

# Create placeholder directories
RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin

# Status port 8090 is exposed by default
EXPOSE 8090

ENTRYPOINT ["/usr/sbin/start_esp.py"]
