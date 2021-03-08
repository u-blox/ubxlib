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
	"encoding/json"
	"flag"
	"io"
	"io/ioutil"
	"log"
	"net"
	"os"
)

// Argument struct for JSON configuration
type Argument struct {
	Verbose    bool   `json:"verbose"`
	Logging    bool   `json:"logging"`
	ServerPort string `json:"server-port"`
}

func echoServerThread(port string, verbose bool) {
	var err error
	log.Println("Opening UDP server listening to port " + port)

	serverAddr, err := net.ResolveUDPAddr("udp", ":" + port)
	if err != nil {
		log.Fatalf("While trying to resolve the port an error occurred %s.", err)
	} else {
		connection, err := net.ListenUDP("udp", serverAddr)
		if err != nil {
			log.Fatalf("While trying to listen for a connection an error occurred %s.", err)
		} else {
			defer connection.Close()
			buffer := make([]byte, 4096)
			for {
				readBytes, addr, err := connection.ReadFromUDP(buffer)
				if err != nil {
					if err != io.EOF {
						log.Printf("Error %s while reading data. Expected an EOF to signal end of connection", err)
					}
					break
				} else {
					log.Printf("Read %d bytes.", readBytes)
					if verbose {
						log.Printf("Message:\n %s\n from %s", buffer, addr)
					}
				}
				writeBytes, err := connection.WriteTo(buffer[:readBytes], addr)
				if err != nil {
					log.Printf("Failed to send data with error: %s ", err)
					break
				}

				if writeBytes != 0 {
					log.Printf("Succesfully echoed back %d bytes.", writeBytes)
				}
			}
		}
	}
}

func startup(config Argument) {
	log.Println("Starting UDP Echo application...")
	echoServerThread(config.ServerPort, config.Verbose)
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
