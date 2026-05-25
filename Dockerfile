FROM ubuntu:22.04

RUN apt update && apt install -y gcc


COPY . .

RUN gcc SenzaTitolo2.c -o server

EXPOSE 5201

CMD ["./server"]