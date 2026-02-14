package main

import (
	"fmt"
	"net/http"
)

func main() {
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintln(w, "Hello from Go HTTP tegfdfgdgdsgdserver!")
	})

	if err := http.ListenAndServe(":9090", nil); err != nil {
		panic(err)
	}
}