#! /bin/bash
ssh ubuntu@192.168.1.69 "cd pico; sudo openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -f openocd.cfg"
