# This small python script was useful while testing TLS
# connections since Python gives quite verbose error messages.
# It is kept here in case it is useful
import socket
import ssl

SEND_DATA = "Hello world"
RECEIVE_DATA = ""
HOST = "ubxlib.redirectme.net"
PORT = 5065
WAIT = True

# Create a TCP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)

# Wrap the socket with SSL.
# See here for the options to the SSL library: https://docs.python.org/3/library/ssl.html.
# The cipher names are those of OpenSSL, which are different to the IANA strings.  For a
# translation table see here: https://testssl.sh/openssl-iana.mapping.html.
secure_sock = ssl.wrap_socket(sock, ssl_version=ssl.PROTOCOL_TLSv1_2)

# Connect and send echo
print(f"Connecting to {HOST}:{PORT}")
secure_sock.connect((HOST, PORT))
print(f"Sending: {SEND_DATA}")
secure_sock.send(SEND_DATA.encode())
print("Receiving: ", end = "")
while WAIT:
    try:
        data = secure_sock.recv(1024)
        if data and len(data) > 0:
            print("{}".format(data.decode()), end = "", flush=True);
        else:
            WAIT = False
    except socket.timeout:
        print("\nEnd")
        WAIT = False

# Close connection
secure_sock.close()