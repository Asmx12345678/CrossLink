FROM gcc:14 AS builder

ARG NS3_VERSION=3.47
ENV DEBIAN_FRONTEND=noninteractive
RUN sed -i 's|http://|https://|g' /etc/apt/sources.list.d/debian.sources && \
    apt-get -o Acquire::Retries=5 update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake ninja-build python3 python3-pip \
    wget bzip2 git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN wget --tries=5 --timeout=30 "https://www.nsnam.org/releases/ns-${NS3_VERSION}.tar.bz2" && \
    tar -xjf "ns-${NS3_VERSION}.tar.bz2" && \
    mv "ns-${NS3_VERSION}" /opt/ns3 && \
    rm -f "ns-${NS3_VERSION}.tar.bz2"

WORKDIR /opt/ns3
RUN ./ns3 configure --build-profile=release --disable-examples --disable-tests \
      --enable-modules=core,network,internet,wifi,mobility,applications,flow-monitor,aodv,olsr,dsdv && \
    ./ns3 build

RUN pip3 install --break-system-packages --no-cache-dir matplotlib==3.10.7

WORKDIR /workspace
COPY . /workspace
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build && ctest --test-dir build --output-on-failure && \
    cp ns3/crosslink-manet.cc /opt/ns3/scratch/crosslink-manet.cc && \
    cd /opt/ns3 && ./ns3 build scratch/crosslink-manet

ENV NS3_HOME=/opt/ns3
ENTRYPOINT ["/workspace/scripts/docker-entrypoint.sh"]
CMD ["demo"]
