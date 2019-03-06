# ESP Proxy that forwards requests into the application server.
FROM ubuntu

# Copy the Endpoints runtime Debian package.
ADD endpoints-runtime.deb endpoints-runtime.deb

# Install dependencies
RUN apt-get update && \
    apt-get install --no-install-recommends -y -q ca-certificates && \
    apt-get -y -q upgrade && \
    apt-get install -y -q --no-install-recommends \
      apt-utils adduser python \
      libc6 libgcc1 libstdc++6 libuuid1 && \
    apt-get clean && rm /var/lib/apt/lists/*_* && \
    dpkg -i /endpoints-runtime.deb && rm /endpoints-runtime.deb

# Create placeholder directories
RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin

# Status port 8090 is exposed by default
EXPOSE 8090

# The default HTTP/1.x port is 8080. It might help to expose this port explicitly
# since documentation still refers to using docker without additional EXPOSE step
EXPOSE 8080

# Remove the nginx user created from deb package which doesn't create the home folder.
RUN deluser nginx && \
    groupadd -g 999 nginx && useradd -r -u 999 -g nginx nginx && \
    rm -rf /var/log/nginx && \
    mkdir -p /var/log/nginx /var/cache/nginx /home/nginx && \
    touch /etc/nginx/server_config.pb.txt && \
    chown nginx:nginx /var/log/nginx /var/cache/nginx /home/nginx /etc/nginx/server_config.pb.txt && \
    chmod 777 /var/log/nginx /var/cache/nginx /home/nginx /etc/nginx/server_config.pb.txt

USER nginx

ENTRYPOINT ["/usr/sbin/start_esp"]
