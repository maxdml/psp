# syntax=docker/dockerfile:1
FROM ubuntu:18.04
RUN apt-get update && apt-get -y update
RUN apt-get install -y build-essential python3.6 python3-pip python3-dev openssh-client vim rsync
RUN mkdir psp
WORKDIR psp
COPY . .
WORKDIR sosp_aec
RUN pip3 install --upgrade pip
RUN pip3 install -r requirements.txt
RUN pip3 install jupyter
RUN mkdir /root/.ssh
RUN echo "host *clemson.cloudlab.us\n\tStrictHostKeyChecking=no" > /root/.ssh/config
CMD ["jupyter", "notebook", "--port=8888", "--no-browser", "--ip=0.0.0.0", "--allow-root"]
