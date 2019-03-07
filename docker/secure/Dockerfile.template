# Build more secure ESP docker image running as non-root
# and allow root filesystem as read-only.

FROM ${PARENT_IMAGE}

RUN rm -rf /var/log/nginx && \
    mkdir -p /var/log/nginx /var/cache/nginx && \
    chown nginx:nginx /var/log/nginx /var/cache/nginx && \
    chmod 777 /var/log/nginx /var/cache/nginx

USER nginx

ENTRYPOINT ["/usr/sbin/start_esp", "--server_config_dir=/home/nginx", "--config_dir=/home/nginx/endpoints"]
