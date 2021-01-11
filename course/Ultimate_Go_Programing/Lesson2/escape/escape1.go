package main

type user struct {
	name  string
	email string
}

func main() {
	u1 := createUser()

	println("u1", &u1)
}

// 这里返回的指针指向栈上分配的user，这会导致逃逸分析生效
// 将user分配在堆上，避免因为函数结束栈被清理导致指针失效。
// 避免内联，否则就没有逃逸分析了
//go:noinline
func createUser() *user {
	u := user{
		name:  "Bill",
		email: "bill@ardanlabs.com",
	}

	println("U", &u)
	return &u
}
