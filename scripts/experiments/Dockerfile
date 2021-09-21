# syntax=docker/dockerfile:1
FROM ubuntu:18.04
RUN apt-get update && apt-get -y update
RUN apt-get install -y build-essential python3.6 python3-pip python3-dev openssh-client vim rsync
RUN mkdir psp
WORKDIR psp
COPY . .
RUN pip3 install --upgrade pip
RUN pip3 install -r scripts/experiments/requirements.txt
RUN pip3 install jupyter
CMD ["jupyter", "notebook", "--port=8888", "--no-browser", "--ip=0.0.0.0", "--allow-root"]
RUN mkdir /root/.ssh
RUN echo "host *clemson.cloudlab.us\n\tStrictHostKeyChecking=no" > /root/.ssh/config
