# ESP Proxy that forwards requests into the application server.
FROM debian:jessie

ADD endpoints-server-proxy.deb endpoints-runtime.deb
ADD ingress ingress
ADD nginx.tmpl /etc/nginx/nginx.tmpl

RUN apt-get update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
      apt-utils adduser curl wget \
      libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_* && \
    dpkg -i /endpoints-runtime.deb && rm /endpoints-runtime.deb

RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin && \
    mkdir -p /etc/nginx/endpoints

ENTRYPOINT ["/ingress"]
