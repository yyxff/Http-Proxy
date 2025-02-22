FROM gcc:latest

RUN apt-get update && \
    apt-get install -y \
    cmake \
    make \
    g++

WORKDIR /app
COPY . /app/

RUN mkdir build && \
    cd build && \
    cmake .. && \
    make

RUN mkdir -p /var/log/erss && \
    chmod 777 /var/log/erss

EXPOSE 12345

CMD ["./build/proxy"]
