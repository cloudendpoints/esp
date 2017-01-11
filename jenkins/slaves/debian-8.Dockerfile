# Stored at gcr.io/endpoints-jenkins/debian-8-slave:latest
FROM debian:jessie-backports

# Please make sure that you update script/linux-install-software as well.
ENV JENKINS_SLAVE_VERSION 2.62
# Docker version needs to match GKE (Docker Host)
ENV DOCKER_VERSION 1.11.2
# Bucket used to store already built binaries
ARG TOOLS_BUCKET

# Installing necessary packages
RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq \
    && apt-get install -qqy git iptables procps sudo xz-utils \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Adding sudo group user no password access.
# This is used by Jenkins user to start docker service
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Installing Tools
ADD script /tmp/esp_tmp/script
RUN chmod +x /tmp/esp_tmp/script/linux-install-software
RUN /tmp/esp_tmp/script/linux-install-software \
      -p "debian-8" \
      -b "${TOOLS_BUCKET}" \
    && rm -rf /tmp/esp_tmp

ENV PATH /usr/lib/google-cloud-sdk/bin:${PATH}

# Setting up jnlp
ENV HOME /home/jenkins

RUN useradd -c "Jenkins user" -d ${HOME} -G docker,sudo -m jenkins -s /bin/bash

ADD http://repo.jenkins-ci.org/public/org/jenkins-ci/main/remoting/${JENKINS_SLAVE_VERSION}/remoting-${JENKINS_SLAVE_VERSION}.jar /tmp/slave.jar
RUN mkdir -p /usr/share/jenkins \
    && chmod 755 /usr/share/jenkins \
    && cp /tmp/slave.jar /usr/share/jenkins/slave.jar \
    && chmod 644 /usr/share/jenkins/slave.jar \
    && rm -rf /tmp/slave.jar

ADD jenkins/slaves/jenkins-slave /usr/local/bin/jenkins-slave
ADD jenkins/slaves/entrypoint /usr/local/bin/entrypoint
RUN chmod +rx /usr/local/bin/jenkins-slave /usr/local/bin/entrypoint

USER jenkins
RUN mkdir ${HOME}/.jenkins
VOLUME ${HOME}/.jenkins
WORKDIR ${HOME}

ENTRYPOINT ["entrypoint"]
