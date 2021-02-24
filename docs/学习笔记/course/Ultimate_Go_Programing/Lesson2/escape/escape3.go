package main

import (
	"bytes"
)

//go:noinline
func InterfaceMethod(value interface{}) {
}

func main() {
	size := 10
	b := make([]byte, size)
	c := bytes.NewBuffer(b)
	c.WriteString("test")
	// 导致逃逸分析
	InterfaceMethod(c)
}
