#!/bin/bash

/home/sven/Projekte/libftdi/ftd2xx/release/examples/BitMode/uart2pts >uart.log 2>.pts &
sleep 1s

lpc21isp -bin test.bin $(cat .pts) 19200 0
