/*
 * FreeRTOS Echo Server V2.0.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

package main

import (
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"flag"
	"io"
	"io/ioutil"
	"log"
	"net"
	"os"
	"time"
	"context"
	"github.com/pion/dtls/v2"
	"github.com/pion/dtls/v2/pkg/protocol"
	"github.com/pion/dtls/v2/pkg/protocol/recordlayer"
	"github.com/pion/transport/v2/udp"
)

// Argument struct for JSON configuration
type Argument struct {
	Verbose    bool   `json:"verbose"`
	Logging    bool   `json:"logging"`
	Secure     bool   `json:"secure-connection"`
	ServerPort string `json:"server-port"`
	ServerCert string `json:"server-certificate-location"`
	ServerKey  string `json:"server-key-location"`
	CACert     string `json:"ca-certificate-location"`
}

// A DTLS or plain UDP listener
type listener struct {
	secure bool
	config dtls.Config
	parent net.Listener
}

// Create a DTLS listener
func LocalListen(network string, laddr *net.UDPAddr, dtlsConfig *dtls.Config) (*listener, error) {
	var l listener
	lc := udp.ListenConfig{}
	if dtlsConfig != nil {
		l.secure = true
		l.config = *dtlsConfig
		lc.AcceptFilter = func(packet []byte) bool {
			pkts, err := recordlayer.UnpackDatagram(packet)
			if err != nil || len(pkts) < 1 {
				return false
			}
			h := &recordlayer.Header{}
			if err := h.Unmarshal(pkts[0]); err != nil {
				return false
			}
			return h.ContentType == protocol.ContentTypeHandshake
		}
	}
	parent, err := lc.Listen(network, laddr)
	if err == nil {
		l.parent = parent
	} else {
		return nil, err
	}
	return &l, nil
}

// Wait for and return the next connection to the listener
func (l *listener) LocalAccept() (net.Conn, error) {
	c, err := l.parent.Accept()
	if err != nil {
		return nil, err
	}
	if !l.secure {
		return c, nil
	}
	return dtls.Server(c, &l.config)
}

// Close the listener.
func (l *listener) LocalClose() error {
	return l.parent.Close()
}

// Return the listener's network address.
func (l *listener) LocalAddr() net.Addr {
	return l.parent.Addr()
}

// Configure security
func secureEcho(serverCertPath string, serverKeyPath string, caCertPath string, port string, verbose bool) (*dtls.Config) {
	// load certificates
	serverCert, err := tls.LoadX509KeyPair(serverCertPath, serverKeyPath)
	if err != nil {
		log.Fatalf("Error %s while loading server certificates", err)
	}

	ca, err := ioutil.ReadFile(caCertPath)
	if err != nil {
		log.Fatalf("Error %s while reading server certificates", err)
	}

	caPool := x509.NewCertPool()
	caPool.AppendCertsFromPEM(ca)

	// Return the DTLS configuration
	return &dtls.Config {
		Certificates:         []tls.Certificate{serverCert},
		// Two cipher suites that our modules support with elliptic curve since
		// pion only supports elliptic curve ciphers for certificate-based authentication
		CipherSuites:         []dtls.CipherSuiteID{dtls.TLS_ECDHE_ECDSA_WITH_AES_128_CCM, dtls.TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8},
		ClientAuth:           dtls.RequireAnyClientCert,
		RootCAs:              caPool,
		ClientCAs:            caPool,
	}
}

// Echo received packets on a connection
func echoPackets(connection net.Conn, verbose bool) {
	defer connection.Close()
	buffer := make([]byte, 4096)
	for {
		readBytes, err := connection.Read(buffer)
		if err != nil {
			if err != io.EOF {
				log.Printf("Error \"%s\" while reading data. Expected an EOF to signal end of connection", err)
			}
			break
		} else {
			log.Printf("Read %d bytes.", readBytes)
			if verbose {
				log.Printf("Message:\n %s\n from %s", buffer, connection.RemoteAddr())
			}
		}
		writeBytes, err := connection.Write(buffer[:readBytes])
		if err != nil {
			log.Printf("Failed to send data with error: \"%s\" ", err)
			break
		}

		if writeBytes != 0 {
			log.Printf("Succesfully echoed back %d bytes.", writeBytes)
		}
	}
}

// The echo server thread which accepts connections
func echoServerThread(port string, dtlsConfig *dtls.Config, verbose bool) {
	log.Println("Opening UDP server listening to port " + port)
	serverAddr, err := net.ResolveUDPAddr("udp", ":" + port)
	if err != nil {
		log.Fatalf("While trying to resolve the port an error occurred %s.", err)
	} else {
		localListener, err := LocalListen("udp", serverAddr, dtlsConfig)
		if err != nil {
			log.Fatalf("Unable to listen on port %s (%s).", port, err)
		} else {
			if dtlsConfig != nil {
				// Create parent context to cleanup handshaking connections on exit.
				ctx, cancel := context.WithCancel(context.Background())
				defer cancel()
				// Create timeout context for accepted connection.
				dtlsConfig.ConnectContextMaker =  func() (context.Context, func()) {
					return context.WithTimeout(ctx, 30*time.Second)
				}
				localListener.config = *dtlsConfig
				localListener.secure = true
			}
			defer localListener.LocalClose()
			for {
				// Wait for a connection.
				connection, err := localListener.LocalAccept()
				if err == nil {
					// Handle the connection in a new goroutine.
					// The loop then returns to accepting, so that
					// multiple connections may be served concurrently.
					go echoPackets(connection, verbose)
				} else {
					log.Printf("Error %s when accepting connection.", err)
				}
			}
		}
	}
}

func startup(config Argument) {
	log.Println("Starting UDP Echo application...")
	if config.Secure {
		log.Println("Security will be used.")
		dtlsConfig := secureEcho(config.ServerCert, config.ServerKey, config.CACert, config.ServerPort, config.Verbose)
		echoServerThread(config.ServerPort, dtlsConfig, config.Verbose)
	} else {
		echoServerThread(config.ServerPort, nil, config.Verbose)
	}
}

func logSetup() {
	echoLogFile, e := os.OpenFile("echo_server.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if e != nil {
		log.Fatal("Failed to open log file.")
	}
	//defer echoLogFile.Close()

	multi := io.MultiWriter(echoLogFile, os.Stdout)
	log.SetOutput(multi)
}

func main() {
	configLocation := flag.String("config", "./config.json", "Path to a JSON configuration.")
	flag.Parse()
	jsonFile, err := os.Open(*configLocation)

	if err != nil {
		log.Fatalf("Failed to open file with error: %s", err)
	}
	defer jsonFile.Close()

	byteValue, _ := ioutil.ReadAll(jsonFile)

	var config Argument
	err = json.Unmarshal(byteValue, &config)
	if err != nil {
		log.Fatalf("Failed to unmarshal json with error: %s", err)
	}

	if config.Logging {
		logSetup()
	}

	startup(config)
}
