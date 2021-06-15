#!/bin/bash

# run ./gunrock_web -t 2 -b 10

for ((i = 0; i < 10; i++));
        do
                curl http://localhost:8080/hello_world.html
                curl http://localhost:8080/hello_world2.html
		curl http://localhost:8080/hello_world3.html
        done

