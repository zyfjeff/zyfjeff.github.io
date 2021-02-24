package flaws

import "testing"

func BenchmarkSliceMapAssignment(b *testing.B) {
	for i := 0; i < b.N; i++ {
		m := make(map[int]*int)
		var x1 int
		m[0] = &x1 // BAD: cause of x1 escape

		s := make([]*int, 1)
		var x2 int
		s[0] = &x2 // BAD: cause of x2 escape
	}
}
