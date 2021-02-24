package main

import (
	"fmt"
	"unsafe"
)

type Example struct {
	BoolValue bool
}

func main() {
	example := &Example{
		BoolValue: true,
	}

	exampleNext := &Example{
		BoolValue: true,
	}

	var zeroExample Example
	zeroExample.BoolValue = false

	alignmentBoundary := unsafe.Alignof(example)

	sizeBool := unsafe.Sizeof(example.BoolValue)
	offsetBool := unsafe.Offsetof(example.BoolValue)

	sizeBoolNext := unsafe.Sizeof(exampleNext.BoolValue)
	offsetBoolNext := unsafe.Offsetof(exampleNext.BoolValue)
	fmt.Printf("example size: %d\n", unsafe.Sizeof(zeroExample))
	fmt.Printf("Alignment Boundary: %d\n", alignmentBoundary)

	fmt.Printf("BoolValue = Size: %d Offset: %d Addr: %v\n",
		sizeBool, offsetBool, &example.BoolValue)

	fmt.Printf("Next = Size: %d Offset: %d Addr: %v\n",
		sizeBoolNext, offsetBoolNext, &exampleNext.BoolValue)
}
