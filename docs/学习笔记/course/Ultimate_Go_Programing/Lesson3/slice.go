package main

import "fmt"

func main() {
	orgSlice := make([]string, 5, 8)
	orgSlice[0] = "Apple"
	orgSlice[1] = "Orange"
	orgSlice[2] = "Banana"
	orgSlice[3] = "Grape"
	orgSlice[4] = "Plum"
	fmt.Printf("cap: %d, length: %d\n", cap(orgSlice), len(orgSlice))

	slice2 := orgSlice[2:4]
	fmt.Printf("cap: %d, length: %d\n", cap(slice2), len(slice2))

	slice2 = append(slice2, "test")
	fmt.Printf("slice2: %v\n", slice2)
	fmt.Printf("orgSlice: %v %d\n", orgSlice, len(orgSlice))

	slice3 := orgSlice[2:]
	fmt.Printf("cap: %d, length: %d\n", cap(slice3), len(slice3))
	slice3 = append(slice3, "test3")
	fmt.Printf("slice3: %v\n", slice3)
	fmt.Printf("orgSlice: %v %d\n", orgSlice, len(orgSlice))

	

}
