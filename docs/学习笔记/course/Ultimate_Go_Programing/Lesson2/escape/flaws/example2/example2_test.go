package flaws

import (
	"testing"
)

func BenchmarkLiteralFunctions(b *testing.B) {
	for i := 0; i < b.N; i++ {
		var y1 int
		foo(&y1, 42) // GOOD: y1 does not escape

		var y2 int
		func(p *int, x int) {
			*p = x
		}(&y2, 42) // BAD: Cause of y2 escape

		var y3 int
		p := foo
		p(&y3, 42) // BAD: Cause of y3 escape
	}
}

func foo(p *int, x int) {
	*p = x
}
