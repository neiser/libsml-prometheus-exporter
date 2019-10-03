#!/bin/sh
./build/libsml-prometheus-exporter -d /dev/ttyUSB1 \
  -b '127.0.0.1:8080' \
  -p 'smartmeter_home_' \
  -m '1-0:1.8.0*255=energy' \
  -m '1-0:1.8.1*255=energy_ch1' \
  -m '1-0:1.8.2*255=energy_ch2' \
  -m '1-0:2.8.0*255=energy_provided' \
  -m '1-0:2.8.1*255=energy_provided_ch1' \
  -m '1-0:2.8.2*255=energy_provided_ch2' \
  -m '1-0:16.7.0*255=power' \
  -m '1-0:36.7.0*255=power_L1' \
  -m '1-0:56.7.0*255=power_L2' \
  -m '1-0:76.7.0*255=power_L3'
