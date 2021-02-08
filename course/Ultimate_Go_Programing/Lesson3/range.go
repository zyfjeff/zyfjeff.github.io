package main

import (
	"fmt"
)

type Dog struct {
	Name string
	Age  int
}

func main() {
	jackie := Dog{
		Name: "Jackie",
		Age:  19,
	}

	fmt.Printf("Jackie Addr: %p\n", &jackie)

	sammy := Dog{
		Name: "Sammy",
		Age:  10,
	}

	fmt.Printf("Sammy Addr: %p\n", &sammy)

	dogs := []Dog{jackie, sammy}

	fmt.Println("")

	for _, dog := range dogs {
		fmt.Printf("Name: %s Age: %d\n", dog.Name, dog.Age)
		fmt.Printf("Addr: %p\n", &dog)

		fmt.Println("")
	}
}
