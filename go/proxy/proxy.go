// Copyright 2020 Google LLC. All Rights Reserved.

// proxy.go is an HTTP/1.1 to HTTP/2.0 proxy to reduce the number of
// connections made by compiler_proxy.
// Note that Go http library automatically use HTTP/2.0 if the protocol is
// supported by a server.
package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"net/http/httputil"
	"os"
	"os/signal"
	"time"
)

var (
	serverHost = flag.String("server-host", "goma.chromium.org", "server host to connect to")
	listenPort = flag.Int("port", 19080, "port to listen connections from compiler_proxy")
)

func main() {
	signal.Ignore(os.Interrupt)
	log.SetOutput(os.Stderr)
	flag.Parse()
	fixResourceLimit()
	listenHostPort := fmt.Sprintf("127.0.0.1:%d", *listenPort)
	t := http.DefaultTransport.(*http.Transport).Clone()
	// http.DefaultTransport with longer timeout in dailer.
	// https://golang.org/pkg/net/http/#DefaultTransport
	// http://b/180579630
	t.DialContext = (&net.Dialer{
		Timeout:   600 * time.Second,
		KeepAlive: 30 * time.Second,
	}).DialContext
	// use http2 is the reason we need this binary.
	t.ForceAttemptHTTP2 = true
	// limit outgoing conn
	// http://b/185719838 http_proxy: too many open files
	t.MaxConnsPerHost = 16
	proxy := &httputil.ReverseProxy{
		Director: func(req *http.Request) {
			req.URL.Scheme = "https"
			req.URL.Host = *serverHost
			req.Host = *serverHost
		},
		Transport: t,
	}
	if err := http.ListenAndServe(listenHostPort, proxy); err != nil {
		log.Fatal(err)
	}
}
