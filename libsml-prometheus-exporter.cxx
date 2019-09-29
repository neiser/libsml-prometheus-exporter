// Copyright 2011 Juri Glass, Mathias Runge, Nadim El Sayed
// DAI-Labor, TU-Berlin
//
// This file is part of libSML.
// Thanks to Thomas Binder and Axel (tuxedo) for providing code how to
// print OBIS data (see transport_receiver()).
// https://community.openhab.org/t/using-a-power-meter-sml-with-openhab/21923
//
// libSML is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libSML is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libSML.  If not, see <http://www.gnu.org/licenses/>.

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <sml/sml_file.h>
#include <sml/sml_transport.h>
#include <sml/sml_value.h>

#include <tclap/CmdLine.h>

#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include "unit.h"

int serial_port_open(const char *device) {
  int bits;
  struct termios config;
  memset(&config, 0, sizeof(config));

  if (!strcmp(device, "-"))
    return 0; // read stdin when "-" is given for the device

#ifdef O_NONBLOCK
  int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
#else
  int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
#endif
  if (fd < 0) {
    fprintf(stderr, "error: open(%s): %s\n", device, strerror(errno));
    return -1;
  }

  // set RTS
  ioctl(fd, TIOCMGET, &bits);
  bits |= TIOCM_RTS;
  ioctl(fd, TIOCMSET, &bits);

  tcgetattr(fd, &config);

  // set 8-N-1
  config.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  config.c_oflag &= ~OPOST;
  config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  config.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
  config.c_cflag |= CS8;

  // set speed to 9600 baud
  cfsetispeed(&config, B9600);
  cfsetospeed(&config, B9600);

  tcsetattr(fd, TCSANOW, &config);
  return fd;
}

void transport_receiver(unsigned char *buffer, size_t buffer_len) {
  int i;
  // the buffer contains the whole message, with transport escape sequences.
  // these escape sequences are stripped here.
  sml_file *file = sml_file_parse(buffer + 8, buffer_len - 16);
  // the sml file is parsed now

  // read here some values ...
  for (i = 0; i < file->messages_len; i++) {
    sml_message *message = file->messages[i];
    if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE) {
      sml_list *entry;
      sml_get_list_response *body;
      body = (sml_get_list_response *)message->message_body->data;
      for (entry = body->val_list; entry != NULL; entry = entry->next) {
        if (!entry->value) { // do not crash on null value
          fprintf(stderr, "Error in data stream. entry->value should not be "
                          "NULL. Skipping this.\n");
          continue;
        }
        if (entry->value->type == SML_TYPE_OCTET_STRING) {
          char *str;
          printf("%d-%d:%d.%d.%d*%d#%s#\n", entry->obj_name->str[0],
                 entry->obj_name->str[1], entry->obj_name->str[2],
                 entry->obj_name->str[3], entry->obj_name->str[4],
                 entry->obj_name->str[5],
                 sml_value_to_strhex(entry->value, &str, true));
          free(str);
        } else if (entry->value->type == SML_TYPE_BOOLEAN) {
          printf("%d-%d:%d.%d.%d*%d#%s#\n", entry->obj_name->str[0],
                 entry->obj_name->str[1], entry->obj_name->str[2],
                 entry->obj_name->str[3], entry->obj_name->str[4],
                 entry->obj_name->str[5],
                 entry->value->data.boolean ? "true" : "false");
        } else if (((entry->value->type & SML_TYPE_FIELD) ==
                    SML_TYPE_INTEGER) ||
                   ((entry->value->type & SML_TYPE_FIELD) ==
                    SML_TYPE_UNSIGNED)) {
          double value = sml_value_to_double(entry->value);
          int scaler = (entry->scaler) ? *entry->scaler : 0;
          int prec = -scaler;
          if (prec < 0)
            prec = 0;
          value = value * pow(10, scaler);
          printf("%d-%d:%d.%d.%d*%d#%.*f#", entry->obj_name->str[0],
                 entry->obj_name->str[1], entry->obj_name->str[2],
                 entry->obj_name->str[3], entry->obj_name->str[4],
                 entry->obj_name->str[5], prec, value);
          const char *unit = NULL;
          if (entry->unit && // do not crash on null (unit is optional)
              (unit = dlms_get_unit((unsigned char)*entry->unit)) != NULL)
            printf("%s", unit);
          printf("\n");
          // flush the stdout puffer, that pipes work without waiting
          fflush(stdout);
        }
      }
    }
  }

  // free the malloc'd memory
  sml_file_free(file);
}

int main(int argc, char *argv[]) {
  try {
    TCLAP::CmdLine cmd("libsml-prometheus-exporter", ' ', "0.1");

    TCLAP::ValueArg<std::string> arg_device(
        "d", "device", "Device to read from", false, "/dev/ttyUSB0", "string");
    cmd.add(arg_device);

    TCLAP::MultiArg<std::string> arg_metric(
        "m", "metric", "Specify metric to OBIS ID", false, "int");
    cmd.add(arg_metric);

    cmd.parse(argc, argv);

    prometheus::Exposer exposer{"127.0.0.1:8080"};

    auto registry = std::make_shared<prometheus::Registry>();

    // add a new counter family to the registry (families combine values with
    // the same name, but distinct label dimensions)
    auto &gauge_family = prometheus::BuildGauge()
                             .Name("time_running_seconds_total")
                             .Help("How many seconds is this server running?")
                             .Register(*registry);

    auto &gauge = gauge_family.Add({});

    exposer.RegisterCollectable(registry);

    // open serial port
    int fd = serial_port_open(arg_device.getValue().c_str());
    if (fd < 0) {
      // error message is printed by serial_port_open()
      exit(1);
    }

    gauge.Set(42);

    // listen on the serial device, this call is blocking.
    sml_transport_listen(fd, &transport_receiver);
    close(fd);

    return 0;

  } catch (TCLAP::ArgException &e) // catch any exceptions
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId()
              << std::endl;
  }
}
