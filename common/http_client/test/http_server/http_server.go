/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief HTTP server used for testing the ubxlib HTTP client,
 * implemented in go, see README.md for how to build and run.
 *
 * HEAD, PUT, POST, GET and DELETE requests are accepted; PUT and POST
 * simply write the body to file, GET retrieves the file, DELETE deletes
 * the file or the file is automatically deleted some time (default 60
 * seconds) after it was written.  File size is limited (default 10 kbytes)
 * and a 1 second delay to responses is applied if more than 1000 files
 * are currently present.
 */

package main

import (
    "strconv"
    "os"
    "os/signal"
    "io"
    "syscall"
    "fmt"
    "flag"
    "sync"
    "time"
    "context"
    "container/list"
    "path/filepath"
    "sort"
    "net"
    "net/http"
)

/* ----------------------------------------------------------------
 * CONSTS
 * -------------------------------------------------------------- */

// Standard port numbers.
const DEFAUT_PORT_HTTP = 80
const DEFAUT_PORT_HTTPS = 443

// Default file deletion delay.
const DEFAULT_FILE_DELETE_DELAY = 60 * time.Second

// Default PUT/POST maximum file size.
const DEFAULT_FILE_SIZE_MAX = 1024 * 10

// The default data directory.
const DEFAULT_DATA_DIR = "."

// Response limiting timeout.
const RESPONSE_LIMIT_DELAY = 1 * time.Second

// The number of files that must be present for RESPONSE_LIMIT_DELAY to kick in.
const RESPONSE_LIMIT_THRESHOLD_FILES = 1000

// Just so we don't suffer from mistyping...
const PARAMETERS_KEY = "parameters"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// Storage for parameters that need to be carried around in a context.
type Parameters struct {
    dataDir string
    maxFileLength int64
    pathList *list.List
    listMutex sync.Mutex
    pResponseDelay *time.Duration
}

// Struct to store a path with creation time so that we can delete it later.
type PathDelete struct {
   path string
   timeCreated time.Time
}

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

// Handler for all HTTP requests.
func handler(response http.ResponseWriter, request *http.Request) {
    var pathDelete PathDelete
    parameters := request.Context().Value(PARAMETERS_KEY).(Parameters)
    fmt.Printf("Received HTTP request type \"%s\", path \"%s\".\n", request.Method, request.URL.String())
    path := filepath.Join(parameters.dataDir, request.URL.String())
    switch request.Method {
        case "HEAD":
            // Nothing to do
        case "GET":
            fmt.Printf("Attempting to serve file \"%s\".\n", path)
            http.ServeFile(response, request, path)
        case "DELETE":
            fmt.Printf("Attempting to delete file \"%s\".\n", path)
            os.Remove(path)
        case "PUT":
            fallthrough
        case "POST":
            fmt.Printf("Attempting to write file \"%s\".\n", path)
            request.Body = http.MaxBytesReader(response, request.Body, parameters.maxFileLength) 

            parameters.listMutex.Lock()
            defer parameters.listMutex.Unlock()

            if err := os.MkdirAll(filepath.Dir(path), 0770); err == nil {
                if file, err := os.Create(path); err == nil {
                    defer file.Close()
                    _, err = io.Copy(file, request.Body)
                    // Add the file to the list of paths to delete
                    pathDelete.path = path
                    pathDelete.timeCreated = time.Now()
                    parameters.pathList.PushFront(pathDelete)
                }
            }
        default:
            fmt.Printf("Unsupported HTTP request type \"%s\".\n", request.Method)
    }
    time.Sleep(*parameters.pResponseDelay)
}

// Asynchronous function to delete paths after a time delay.
func deletePaths(ctx context.Context, pDeleteDelay *time.Duration, pKeepGoing *bool){
    parameters := ctx.Value(PARAMETERS_KEY).(Parameters)
    var next *list.Element

    for *pKeepGoing {

        parameters.listMutex.Lock()

        numFiles := 0
        for x := parameters.pathList.Front(); x != nil; x = next {
            if time.Now().Sub(x.Value.(PathDelete).timeCreated) > *pDeleteDelay {
                // File is too old, remove it from disk and from the list
                if *pDeleteDelay > 0 {
                    fmt.Printf("File \"%s\" is more than %d second(s) old, deleting...\n",
                               x.Value.(PathDelete).path, *pDeleteDelay / time.Second)
                } else {
                    fmt.Printf("Cleaning up file \"%s\"...\n", x.Value.(PathDelete).path)
                }
                os.Remove(x.Value.(PathDelete).path)
                next = x.Next()
                parameters.pathList.Remove(x)
            } else {
                numFiles++
            }
            next = x.Next()
        }

        // Remove any empty directories; first get a slice of all of the directories
        directories := []string{}
        filepath.Walk(parameters.dataDir, func(path string, info os.FileInfo, err error) error {
            if path != parameters.dataDir && info.IsDir() {
                directories = append(directories, path)
            }
            return nil
        })
        // Sort the slice of directories so that the longest paths are first
        sort.Slice(directories, func(this, next int) bool {
            return len(directories[next]) > len(directories[this])
        })
        // Now run through the sorted list of directories deleting empty ones
        for _, directory := range directories {
            if entry, err := os.Open(directory); err == nil {
                empty := false
                if _, err := entry.Readdir(1); err == io.EOF {
                    empty = true
                }
                entry.Close()
                if empty {
                    fmt.Printf("Removing empty directory \"%s\".\n", directory)
                    os.Remove(directory)
                }
            }
        }
        // Set the rate limiting delay based on the number
        // of files in the list
        if numFiles > RESPONSE_LIMIT_THRESHOLD_FILES {
            *parameters.pResponseDelay = RESPONSE_LIMIT_DELAY
        } else {
            *parameters.pResponseDelay = 0
        }

        parameters.listMutex.Unlock()

        time.Sleep(time.Second)
    }
}

// Entry point.
func main() {
    var parameters Parameters
    var err error
    var port int
    var pCertFile *string
    var pKeyFile *string
    var keepGoing = true
    var deleteDelay time.Duration
    var responseDelay time.Duration

    // Catch exit signal so that we can clean up
    finished := make(chan os.Signal, 1)
    signal.Notify(finished, os.Interrupt, syscall.SIGINT, syscall.SIGTERM)

    // Some command-line flags
    flag.IntVar(&port, "port", DEFAUT_PORT_HTTP, "the port number to listen on")
    pCertFile = flag.String("cert_file", "", "path to the certificate file; required for HTTPS")
    pKeyFile = flag.String("key_file", "", "path to the key file; required for HTTPS")
    flag.DurationVar(&deleteDelay, "delete_delay", DEFAULT_FILE_DELETE_DELAY, "the time until any PUT/POST file is deleted")

    // More command-line flags, this time ones we need to pass to the HTTP request handler
    pDataDir := flag.String("dir", DEFAULT_DATA_DIR, "directory to use as the test HTTP server data area (must exist)")
    flag.Int64Var(&parameters.maxFileLength, "max_file_length", DEFAULT_FILE_SIZE_MAX, "the maximum size of a file being PUT/POST in bytes")

    // Parse the command line to populate the variables
    flag.Parse()

    if parameters.dataDir, err = filepath.Abs(*pDataDir); err == nil {
        responseDelay = 0;
        parameters.pResponseDelay = &responseDelay
        parameters.pathList = list.New()
        // Create a context we can pass to the HTTP request handler
        ctx := context.WithValue(context.Background(), PARAMETERS_KEY, parameters)

        // Start a go-routine which deletes files that have been PUT/POST
        // after a time delay
        go deletePaths(ctx, &deleteDelay, &keepGoing)

        if *pCertFile != "" && *pKeyFile != "" && port == DEFAUT_PORT_HTTP {
            port = DEFAUT_PORT_HTTPS
        }

        // The server, which is given the context
        server := &http.Server{Addr:        ":" + strconv.Itoa(port),
                               ReadTimeout: 10 * time.Second,
                               BaseContext: func(_ net.Listener) context.Context { return ctx }}

        fmt.Printf("Starting HTTP test server on port %d (re-run with -h for command-line help):\n", port)
        if *pCertFile != "" && *pKeyFile != "" {
            fmt.Printf(" - secured (HTTPS) with cerificate file \"%s\", key file \"%s\".\n", *pCertFile, *pKeyFile)
        }
        fmt.Printf(" - data directory will be \"%s\".\n", parameters.dataDir)
        fmt.Printf(" - delete timeout for PUT/POST files will be %d second(s).\n", deleteDelay / time.Second)
        fmt.Printf(" - max PUT/POST file length will be %d byte(s).\n", parameters.maxFileLength)
        fmt.Printf("Use CTRL-C to stop.\n")
        http.HandleFunc("/", handler)
        go func() {
            if *pCertFile != "" && *pKeyFile != "" {
                err = server.ListenAndServeTLS(*pCertFile, *pKeyFile)
            } else {
                err = server.ListenAndServe()
            }
            if err != nil && err != http.ErrServerClosed {
                fmt.Printf("HTTP server failed to start (%s).\n", err)
            }
        }()

        // Wait for CTRL-C
        <-finished
        fmt.Printf("HTTP test server cleaning up...\n")
        // Let the deletePaths go-routine clean-up and then exit
        deleteDelay = 0;
        time.Sleep(2 * time.Second)
        keepGoing = false;
        time.Sleep(100 * time.Millisecond)
        os.Exit(0)
    } else {
        fmt.Printf("Unable to determine current directory, exiting.\n")
    }
}

// End of file
