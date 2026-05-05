FROM gcc:latest

WORKDIR /app

COPY . .

RUN make server_app

EXPOSE 8080

CMD ["./server/server_app"]