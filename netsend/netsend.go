package main

import (
	"fmt"
	"net"
	"os"
	"flag"
)

type flags struct {
    net string
    ip string
    port int
}

var f flags

func init() {
    flag.StringVar(&f.net, "net", "tcp", "network (tcp, udp)")
    flag.StringVar(&f.ip, "ip", "localhost", "ip address")
    flag.IntVar(&f.port, "port", 1234, "ip port")
}

func main() {
    flag.Parse()

/*
    addr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("%s:%d", *ip, *port))
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}

	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
*/

    conn, err := net.Dial(f.net, fmt.Sprintf("%s:%d", f.ip, f.port))
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}

    //sendMany(conn)
    sendOne(conn, "Hello Gophers!")

	err = conn.Close()
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func sendMany(conn net.Conn) {
	c := make(chan byte)

	for i := 0; i < 100; i++ {
		go func(i int) {
		    sendOne(conn, fmt.Sprintf("Hello Gophers!: %d", i))
			c <- 0
		}(i)
	}

	for i := 0; i < 100; i++ {
		<-c
	}
}

func sendOne(conn net.Conn, msg string) {
    _, err := conn.Write([]byte(msg))
    if err != nil {
        fmt.Println(err)
        os.Exit(1)
    }
}
