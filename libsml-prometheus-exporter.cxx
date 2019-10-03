#include <cmath>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include <sml/sml_file.h>
#include <sml/sml_transport.h>
#include <sml/sml_value.h>

#include <tclap/CmdLine.h>

#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include "unit.h"

using namespace std;

map<string, prometheus::Family<prometheus::Gauge>*> gauge_families;

int serial_port_open(const char* device) {
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
  int bits = 0;
  ioctl(fd, TIOCMGET, &bits);
  bits |= TIOCM_RTS;
  ioctl(fd, TIOCMSET, &bits);

  tcgetattr(fd, &config);

  // set 8-N-1
  config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
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

string get_unit(sml_list* entry) {
  if (entry->unit != nullptr) {
    auto it_unit = unit_table.find(*entry->unit);
    if (it_unit != unit_table.cend()) {
      return it_unit->second;
    }
  }
  return "none";
}

string get_obis_id(sml_list* entry) {
  stringstream ss;
  ss << +entry->obj_name->str[0] << "-" << +entry->obj_name->str[1] << ":"
     << +entry->obj_name->str[2] << "." << +entry->obj_name->str[3] << "."
     << +entry->obj_name->str[4] << "*" << +entry->obj_name->str[5];
  return ss.str();
}

void transport_receiver(unsigned char* buffer, size_t buffer_len) {
  // the buffer contains the whole message, with transport escape sequences.
  // these escape sequences are stripped here.
  sml_file* file = sml_file_parse(buffer + 8, buffer_len - 16);

  for (auto& message : vector<sml_message*>{file->messages, file->messages + file->messages_len}) {
    if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE) {
      sml_get_list_response* body = (sml_get_list_response*)message->message_body->data;
      for (sml_list* entry = body->val_list; entry != nullptr; entry = entry->next) {
        if (entry->value == nullptr) {
          cerr << "Error in data stream. entry->value should not be null. "
                  "Skipping this."
               << endl;
          continue;
        }
        if (((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_INTEGER) ||
            ((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_UNSIGNED)) {

          int scaler = entry->scaler != nullptr ? *entry->scaler : 0;
          int prec = -scaler;
          if (prec < 0)
            prec = 0;
          double value = sml_value_to_double(entry->value) * pow(10, scaler);

          string obis_id = get_obis_id(entry);
          string unit = get_unit(entry);

          auto it_gauge_family = gauge_families.find(obis_id);
          if (it_gauge_family == gauge_families.cend()) {
            cerr << "No gauge found for OBIS " << obis_id << " with value " << value << " " << unit
                 << endl;
            continue;
          }

          auto& gauge_family = *it_gauge_family->second;
          gauge_family.Add({{"unit", unit}, {"prec", to_string(prec)}}).Set(value);
        }
      }
    }
  }

  sml_file_free(file);
}

int main(int argc, char* argv[]) {
  try {
    TCLAP::CmdLine cmd("libsml-prometheus-exporter", ' ', "0.1");

    TCLAP::ValueArg<string> arg_device("d", "device", "Device to read from", false, "/dev/ttyUSB0",
                                       "string");
    cmd.add(arg_device);

    TCLAP::ValueArg<string> arg_bindaddress("b", "bind", "Prometheus bind address including port",
                                            false, "127.0.0.1:8080", "address");
    cmd.add(arg_bindaddress);

    TCLAP::ValueArg<string> arg_prefix("p", "prefix", "Prefix for metric names", false, "",
                                       "prefix");
    cmd.add(arg_prefix);

    TCLAP::MultiArg<string> arg_metrics("m", "metric", "Specify metric to OBIS ID", false,
                                        "obis id");
    cmd.add(arg_metrics);

    cmd.parse(argc, argv);

    auto registry = make_shared<prometheus::Registry>();

    for (const auto& arg_metric : arg_metrics.getValue()) {

      size_t idx = arg_metric.find("=");
      if (idx == string::npos) {
        cerr << "Ignoring metric specification without =" << endl;
        continue;
      }

      string obis_id = arg_metric.substr(0, idx);
      string metric_name = arg_prefix.getValue() + arg_metric.substr(idx + 1);

      if (gauge_families.find(obis_id) != gauge_families.cend()) {
        cerr << "Not adding metric for OBIS " << obis_id << " again" << endl;
      }

      auto& gauge_family = prometheus::BuildGauge()
                               .Name(metric_name)
                               .Labels({{"obis_id", obis_id}})
                               .Help("OBIS " + obis_id)
                               .Register(*registry);
      gauge_families.insert(make_pair(obis_id, addressof(gauge_family)));
      cout << "Adding gauge " << metric_name << " for OBIS ID " << obis_id << endl;
    }

    prometheus::Exposer exposer{arg_bindaddress.getValue()};
    exposer.RegisterCollectable(registry);

    cout << "Providing metrics on " << arg_bindaddress.getValue() << endl;

    // open serial port
    int fd = serial_port_open(arg_device.getValue().c_str());
    if (fd < 0) {
      // error message is printed by serial_port_open()
      return 1;
    }

    // listen on the serial device, this call is blocking.
    sml_transport_listen(fd, &transport_receiver);
    close(fd);

    return 0;

  } catch (TCLAP::ArgException& e) // catch any exceptions
  {
    cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
  }
}
