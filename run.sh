umockdev-run -d mt681.umockdev -s /dev/ttyUSB0=ttyUSB0.script -- \
  ./build/libsml-prometheus-exporter -d /dev/ttyUSB0 \
  -p 'smartmeter_home_' \
  -m '1-0:1.8.0*255=total_energy'
