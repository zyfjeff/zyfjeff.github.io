package main

func test() string {
	var b []byte
	b = append(b, "Hello, "...)
	b = append(b, "world.\n"...)
	return string(b) // BAD: allocates again
}

func main() {
	c := test()
}
