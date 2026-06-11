FROM ubuntu:22.04

RUN apt update && apt install -y gcc


COPY . .

RUN gcc SenzaTitolo2.c server_game.c server_net.c -o server -pthread

RUN chmod +x serverImmortale.sh

EXPOSE 5201

CMD ["./serverImmortale.sh"]