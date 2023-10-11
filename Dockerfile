FROM ubuntu:latest
MAINTAINER ben

RUN apt update

RUN apt upgrade -y 

ENTRYPOINT ["/bin/bash"]
