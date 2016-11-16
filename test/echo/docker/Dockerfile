# Copyright (C) Extensible Service Proxy Authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
################################################################################
#
# ESP Proxy that forwards requests into the appengine application.

FROM google/debian:wheezy

# Add the debian:jessie apt repo for newer dependencies that are not in wheezy
RUN echo "deb http://httpredir.debian.org/debian jessie main" >> /etc/apt/sources.list
RUN echo "deb http://security.debian.org/ jessie/updates main" >> /etc/apt/sources.list

# Install all of the needed dependencies
RUN apt-get update && \
    apt-get install -y apt-utils adduser ca-certificates wget && \
    apt-get install -y nginx-common libpcre3 geoip-database libjpeg8 \
                       libpng12-0 libgd2-noxpm libxml2 libxslt1.1 sgml-base \
                       libgeoip1 xml-core libexpat1 && \
    apt-get clean && \
    rm /var/lib/apt/lists/*_*

# Download and install our own build of nginx
ADD endpoints-runtime.deb endpoints-runtime.deb
RUN dpkg -i endpoints-runtime.deb && \
    rm /etc/nginx.conf /endpoints-runtime.deb

RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin

ADD nginx.conf /etc/nginx.conf
ADD static.conf /var/lib/nginx/optional/static.conf
ADD start_nginx.sh /var/lib/nginx/bin/start_nginx.sh
ADD service.json /etc/service.json

EXPOSE 8080
EXPOSE 8090

# to run: docker run --link gaeapp:gaeapp -p 8080:8080 --expose 8090

ENTRYPOINT ["/var/lib/nginx/bin/start_nginx.sh"]
