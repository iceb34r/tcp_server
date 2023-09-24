# TCP server
TCP messaging server written with [`liburing`](https://github.com/axboe/liburing)
## Installing
- Install and build [`liburing`](https://github.com/axboe/liburing) from source:

```bash
git clone https://github.com/axboe/liburing

cd liburing

./configure
make
make install
```
- Install TCP server:
```bash
git clone https://github.com/iceb34r/tcp_server

mkdir build && cd build
cmake ..
make
```

## Usage example
- Run server with  `./server [port]` command. It will create `<port>.txt` file
 ```bash
$ ./server 14545
server listening on port: 14545
  ```

- Connect to the server with telnet and send any message
```bash
$ telnet 127.0.0.1 14545
Trying 127.0.0.1
Connected to 127.0.0.1
Escape character is '^]'
testmsg
```
- The message will be placed in a `<port>.txt` file
