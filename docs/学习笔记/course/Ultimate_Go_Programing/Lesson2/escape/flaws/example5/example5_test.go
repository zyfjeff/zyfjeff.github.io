package flaws

import (
	"bytes"
	"testing"
)

func BenchmarkUnknown(b *testing.B) {
	for i := 0; i < b.N; i++ {
		var buf bytes.Buffer
		buf.Write([]byte{1})
		_ = buf.Bytes()
	}
}
