# streamer

Ubuntu 18.04

## For build
    requirements: ffmpeg, boost, gtest

    git clone git@github.com:gvi82/streamer.git
    cd streamer
    mkdir build
    cd build
    cmake ..
    make

## Server
    # increase system buffer
    echo 2097152 > /proc/sys/net/core/rmem_max

    cd server
    ./server
    Runing on 8080 port

## Client
    ./client /path/to/file.mp4
    params:
        -server 127.0.0.1 -- server ip address
        -port 8080 -- server port

## Tests
    copy test_video.mp4 to dir

    start 5 simultaneous streams
    ./runTests

