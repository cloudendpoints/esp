# Stored at gcr.io/endpoints-jenkins/bazel-worker:latest
FROM debian:jessie-backports

# Please make sure that you update script/linux-install-software as well.
ENV JAVA_VERSION 1.8
ENV JENKINS_SLAVE_VERSION 2.60

# Installing necessary packages
RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq && apt-get install -qqy \
    bash-completion \
    curl \
    iptables \
    python-setuptools \
    sudo \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Adding sudo group user no password access.
# This is used by Jenkins user to start docker service
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Installing gcloud and kubectl to interact with Kuybernetes
RUN rm -rf /usr/lib/google-cloud-sdk \
    && export CLOUDSDK_CORE_DISABLE_PROMPTS=1 \
    && export CLOUDSDK_INSTALL_DIR=/usr/lib/ \
    && curl https://sdk.cloud.google.com | bash \
    && ln -s /usr/lib/google-cloud-sdk/bin/gcloud /usr/bin/gcloud \
    && gcloud components update kubectl alpha -q

ENV PATH /usr/lib/google-cloud-sdk/bin:$PATH

# gcloud needs a UTF-8 locale
ENV LANG C.UTF-8

# Installing Tools
ADD script /tmp/esp_tmp/script
RUN chmod +x /tmp/esp_tmp/script/linux-install-software
RUN /tmp/esp_tmp/script/linux-install-software -d \
    && rm -rf /tmp/esp_tmp

# Docker settings.
VOLUME /var/lib/docker
EXPOSE 2375

# Setting up jnlp
ENV HOME /home/jenkins

RUN useradd -c "Jenkins user" -d $HOME -G docker,sudo -m jenkins -s /bin/bash
RUN curl --create-dirs -sSLo /usr/share/jenkins/slave.jar \
    http://repo.jenkins-ci.org/public/org/jenkins-ci/main/remoting/${JENKINS_SLAVE_VERSION}/remoting-${JENKINS_SLAVE_VERSION}.jar \
    && chmod 755 /usr/share/jenkins \
    && chmod 644 /usr/share/jenkins/slave.jar

ADD jenkins/slaves/jenkins-slave /usr/local/bin/jenkins-slave
RUN chmod +rx /usr/local/bin/jenkins-slave

USER jenkins
VOLUME /home/jenkins
WORKDIR /home/jenkins

ENTRYPOINT ["jenkins-slave"]
