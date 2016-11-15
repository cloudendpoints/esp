// Copyright (C) Extensible Service Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//

package deploy

import (
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"strings"
	"fmt"
	"syscall"
	"time"
)

const serviceConfFileName = "service.pb.txt"
const nginxConfFileName = "nginx.conf"
const nginxPidFileName = "nginx.pid"

func createSandboxFile(sandboxDir, name, conf string) error {
	absPath := sandboxDir + "/" + name
	out, err := os.Create(absPath)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = out.WriteString(conf)

	if err == nil {
		log.Println("Created file: " + name + " in sandbox: ", )
	}

	return err
}

func nginxConfFilePath(sandboxDir string) string {
	return sandboxDir + "/" + nginxConfFileName
}

func nginxPidFilePath(sandboxDir string) string {
	return sandboxDir + "/" + nginxPidFileName
}

func CreateSandbox(parentDir string) (sandboxDir string, err error) {

	// Create tmp directory under TEST_RUNS_DIR
	sandboxDir, err = ioutil.TempDir(parentDir, "esp-test-")
	if err != nil {
		log.Fatal("Error creating tmp dir: ", err)
		return "", err
	}

	log.Println("Created test tmp dir = ", sandboxDir)

	return sandboxDir, err
}

func PrepareSandbox(sandboxDir, nginxConf, serviceConf string) error {

	err := createSandboxFile(sandboxDir, nginxConfFileName, nginxConf)

	if err != nil {
		log.Fatal("Error creating nginx src conf file: ", err)
		return err
	}

	err = createSandboxFile(sandboxDir, serviceConfFileName, serviceConf)

	if err != nil {
		log.Fatal("Error creating services conf file: ", err)
		return err
	}

	return err
}

func GetNginxPid(sandboxDir string) (pid int, err error) {
	f, err := os.Open(nginxPidFilePath(sandboxDir))
	if err != nil {
		return -1, err
	}
	defer f.Close()

	_, err = fmt.Fscanf(f, "%d", &pid)
	return pid, err
}

func StartEsp(espExecFile, sandboxDir string, timeout int) (pid int, err error) {
	cmd := exec.Command(espExecFile, "-p", sandboxDir, "-c", nginxConfFilePath(sandboxDir))
	err = cmd.Start()

	if err != nil {
		log.Fatal("Failed to start ESP executable")
		return -1, err
	}

	log.Println("Starting ESP, cmd line: ", strings.Join(cmd.Args, " "))

	for i := 0; i < timeout * 10; i++ {
		pid, err = GetNginxPid(sandboxDir)
		if err == nil {
			break
		}

		log.Println("Waiting for ESP start")
		time.Sleep(100 * time.Millisecond)
	}

	if err == nil {
		log.Println("Success, PID=", pid)
	} else {
		log.Println("FAILED, error=", err)
	}

	return pid, err
}

func StopEsp(sandboxDir string, cleanup bool, timeout int) error {
	pid, err := GetNginxPid(sandboxDir)

	if err == nil {
		err = syscall.Kill(pid, syscall.SIGQUIT)
	}

	if err != nil {
		log.Print("Error stopping ESP:", err)
	}

	for i := 0; i < timeout * 10; i++ {
		pid, err = GetNginxPid(sandboxDir)
		if err != nil {
			break
		}

		log.Println("Waiting for ESP to terminate")
		time.Sleep(100 * time.Millisecond)
	}

	if err != nil {
		log.Println("Success")
	} else {
		log.Println("FAILED to terminate ESP, error=", err)
		return err
	}

	// clean up only if terminating ESP succeeded
	if cleanup {
		log.Println("Cleaning up sandbox: ", sandboxDir)
		err = os.RemoveAll(sandboxDir)
		if err != nil {
			log.Println("Error cleaning up sandbox:", err)
		}
	}

	return err
}

