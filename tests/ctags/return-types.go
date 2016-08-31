package main

func foo() *struct {a int; b func()} {
	return nil
}

func bar() (a int, b func(a int), c interface{}, d string) {
	return 0, nil, nil, ""
}

func baz() *[20*3+1]map[chan<- /* map[int]int */<-chan int]map[interface{}]*string {
	return nil
}

func main() {
}