Files included:

1. server.c
2. peer.c
3. make_peer
4. make_server

Instructions:

SERVER:
1. ./make_server
2. ./server

* This will run the server on port '9967'. Currently, this can be changed by editing SERVER_PORT value in server.c

PEER:
1. ./make_peer
2. ./peer <server_ip> <server_port> <abs_share_path>

* <server_ip> is the ip of the machine the server is running on
* <server_port> is the port on which the server is running on. In our case it's 9967
* <abs_share_path> is the directory path whose files will be shared. Also the files requested will be downloaded into this directory.

FETCHING FILES ON PEER:
1. Suppose a peer is sharing a file called 'File1.txt', then to download that file use, 'fetch File1.txt' (with the file extension) in your peer process.
2. The server will return appropriate things needed to auto initiate the download on the peer end.
3. If there is no such file in any of the peer, the server will return a 404 message and an appropriate error is displayed.


PUBLISHING FILES:
1. The peer process automatically published all the files present in the <abs_share_path>.
2. To publish a new file, use 'publish <file_name> <abs_file_path>', on the peer side. <file_name> should include the extension of the file too. Eg. <file_name> = 'File1.txt'.
3. To publish all files in the <abs_share_path>, use 'publih ALL' command in the peer process.
