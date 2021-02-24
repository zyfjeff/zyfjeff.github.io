package main

import (
	"bytes"
)

func main() {
	size := 10
	b := make([]byte, size)
	c := bytes.NewBuffer(b)
	c.WriteString("test")
}
