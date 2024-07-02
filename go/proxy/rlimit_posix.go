// Copyright 2021 Google LLC. All Rights Reserved.
// +build !windows

package main

import (
	"log"
	"syscall"
)

func fixResourceLimit() {
	var r syscall.Rlimit
	err := syscall.Getrlimit(syscall.RLIMIT_NOFILE, &r)
	if err != nil {
		log.Printf("failed to getrlimit(RLIMIT_NOFILE)=%v", err)
		return
	}
	log.Printf("getrlimit(RLIMIT_NOFILE)={cur: %d, max: %d}", r.Cur, r.Max)
	if r.Cur == r.Max {
		return
	}
	r.Cur = r.Max
	err = syscall.Setrlimit(syscall.RLIMIT_NOFILE, &r)
	if err != nil {
		log.Printf("failed to setrlimit(RLIMIT_NOFILE, {cur: %d, max: %d})=%v", r.Cur, r.Max, err)
		return
	}
	log.Printf("setrlimit(RLIMIT_NOFILE, {cur: %d, max: %d})", r.Cur, r.Max)
}
