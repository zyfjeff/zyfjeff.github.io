package flaws

import "testing"

type Iface interface {
	Method()
}

type X struct {
	name string
}

func (x X) Method() {}

func BenchmarkInterfaces(b *testing.B) {
	for i := 0; i < b.N; i++ {
		x1 := X{"bill"}
		var i1 Iface = x1
		var i2 Iface = &x1

		i1.Method() // BAD: cause copy of x1 to escape
		i2.Method() // BAD: cause x1 to escape

		x2 := X{"bill"}
		foo(x2)
		foo(&x2)
	}
}

func foo(i Iface) {
	i.Method() // BAD: cause value passed in to escape
}
