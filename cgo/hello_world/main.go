package main

/*
#include <stdio.h>
#include <stdlib.h> // This is needed for free()

void printMessage(const char* message) {
    printf("%s\n", message);
}
*/
import "C"

import (
	"fmt"
	"unsafe"
)

func main() {
	message := C.CString("Hello from CGo")
	C.printMessage(message)
	C.free(unsafe.Pointer(message))

	fmt.Println("Back in Go")
}
