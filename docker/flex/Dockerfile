# ESP Proxy that forwards requests into the appengine application.

FROM gcr.io/google-appengine/base

# This file path is currently hard coded because there's no way to pass input
# parameters into Dockerfiles or `docker build`
ADD endpoints-runtime.deb endpoints-runtime.deb

# Install all of the needed dependencies

# Upgrade the OS and core packages,
# install core required utilities,
# install ESP dependencies (libc6 libgcc1 libstdc++6 libuuid1).
# Clean up
RUN apt-get -q update && \
    apt-get install --no-install-recommends -y -q apt-utils ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
        adduser cron logrotate wget && \
    apt-get install -y -q --no-install-recommends \
        libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_* && \
    dpkg -i /endpoints-runtime.deb && rm /endpoints-runtime.deb

RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin && \
    mkdir -p /var/lib/nginx/inc

ADD static.conf /var/lib/nginx/optional/static.conf
ADD start_nginx.sh /var/lib/nginx/bin/start_nginx.sh
ADD proxy_pass.inc /var/lib/nginx/inc/proxy_pass.inc
ADD nginx.logrotate /etc/logrotate.d/nginx

EXPOSE 8080
EXPOSE 8090

# to run: docker run --link gaeapp:gaeapp -p 8080:8080 --expose 8090

ENTRYPOINT ["/var/lib/nginx/bin/start_nginx.sh"]
