PROJECT = endpoints-jenkins
VERSION = 0.10
TOOLS_BUCKET = endpoints-tools

# Note: The build directory is the root of the istio/test-infra repository, not ./
image:
	docker build -t debian-8 --build-arg TOOLS_BUCKET="${TOOLS_BUCKET}" -f debian-8.Dockerfile ../../
	docker tag debian-8 gcr.io/$(PROJECT)/debian-8:$(VERSION)
	docker tag debian-8 gcr.io/$(PROJECT)/debian-8:latest


push:
	gcloud docker -- push gcr.io/$(PROJECT)/debian-8:$(VERSION)
	gcloud docker -- push gcr.io/$(PROJECT)/debian-8:latest


.PHONY: image push
