package main

func main() {
	i := 0
	pp := new(*int)
	*pp = &i // BAD: i escapes
	_ = pp
}
