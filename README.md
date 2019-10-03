# Prometheus exporter for libsml-based Smart Meters

This little program combines [libsml](https://github.com/volkszaehler/libsml)
and [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) to export metrics
read out from Smartmeters such as ISKRA MT681.

## Build

```bash
mkdir build && cd build && cmake .. && make 
``` 

The project fetches external dependencies, and requires the following packages
to be installed (example here for Debian 10, adjust accordingly):

```bash
apt install git build-essential uuid-dev pkg-config libcurl4-openssl-dev zlib1g-dev
```

## Usage

See the provided `run-*.sh` scripts, where one is
using [umockdev](https://github.com/martinpitt/umockdev) in order to run it
without the device present (useful for development).
