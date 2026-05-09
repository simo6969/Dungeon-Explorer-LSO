
FROM gcc:14


WORKDIR /app


COPY . .



RUN make clean && make


EXPOSE 8080



CMD ["./server/server_app"]
