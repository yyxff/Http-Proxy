FROM gcc:latest

RUN apt-get update && \
    apt-get install -y \
    cmake \
    make \
    g++ \
    libboost-dev

WORKDIR /app
COPY ./src /app/src

# RUN mkdir build && \
#     cd build && \
#     cmake .. && \
#     make

RUN mkdir -p /var/log/erss && \
    chmod 777 /var/log/erss

EXPOSE 12345

# CMD ["./build/proxy"]
CMD ["/bin/bash", "-c", "mkdir -p /app/build && cd /app/build && cmake ../src && make && ./proxy"]