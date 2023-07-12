The CA certificate and the signed client and server certificates were generated as follows:

```
openssl ecparam -name  secp384r1 -genkey -noout -out ca_key.pem
openssl req -new -x509 -days 36500 -extensions v3_ca -key ca_key.pem -out ca_cert.pem -subj "/C=US/ST=WA/L=Thalwil/O=u-blox/OU=ca/CN=www.u-blox.com/emailAddress=ubxlib@u-blox.com"

openssl ecparam -name  secp384r1 -genkey -noout -out server_key.pem
openssl req -new -key server_key.pem -out server_csr.pem -subj "/C=US/ST=WA/L=Thalwil/O=u-blox/OU=server/CN=www.u-blox.com/emailAddress=ubxlib@u-blox.com"
openssl x509 -req -in server_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem -days 36500 -sha384

openssl ecparam -name  secp384r1 -genkey -noout -out client_key.pem
openssl req -new -key client_key.pem -out client_csr.pem -subj "/C=US/ST=WA/L=Thalwil/O=u-blox/OU=client/CN=www.u-blox.com/emailAddress=ubxlib@u-blox.com"
openssl x509 -req -in client_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out client_cert.pem -days 36500 -sha384
```

Some notes on why the choices above were made:
- Something must be different in the subject field in each case or the certificate will be rejected.
- Elliptic curve is used throughout as the Golang package we use on the server-side for DTLS testing ([pion](https://pkg.go.dev/github.com/pion/dtls)) only offers eliptic curve ciphers for certificate-based authentication.
- There are limitations to the supported elliptic curves (`secp384r1` etc.) and signature algorithms (the combination of a curve and a SHA) potentially on both the client and server sides; the above work with our test server and the clients we want to test.

\[You can delete `server_csr.pem` and `client_csr.pem` afterwards\].

IMPORTANT: the contents of these files are pasted into [/common/security/test/u_security_tls_test.c](/common/security/test/u_security_tls_test.c) and [/example/sockets/credentials_tls.c](/example/sockets/credentials_tls.c); if you change them here you must change the pasted-in versions to match.