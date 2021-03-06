/* Copyright 2017 Duncan Law (mrdunk@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ESP8266__HOST_ATTRIBUTES_H
#define ESP8266__HOST_ATTRIBUTES_H

#include "config.h"
#include "devices.h"

struct Config {
  char hostname[HOSTNAME_LEN];
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress brokerip;
  int brokerport;
  char subscribeprefix[PREFIX_LEN];
  char publishprefix[PREFIX_LEN];
  Connected_device devices[MAX_DEVICES];
  char firmwarehost[STRING_LEN];
  char firmwaredirectory[STRING_LEN];
  int firmwareport;
  char enablepassphrase[STRING_LEN];
  int enableiopin;
  char wifi_ssid[STRING_LEN];
  char wifi_passwd[STRING_LEN];

  bool setValue(const String& parent,
                const String& key,
                const String& value,
                Connected_device& device);
 bool testValue(const String& parent,
                        const String& key,
                        const String& value);
  void clear();
  bool load(const String& filename="/config.cfg", bool test=false);
  bool save(const String& filename="/config.cfg");
  void insertDevice(Connected_device device);

  int labelToIndex(const int label){
    int count_valid = -1;
    int empty_slot = -1;
    for(int index = 0; index < MAX_DEVICES; index++){
      if(strlen(devices[index].address_segment[0].segment) > 0) {
        count_valid++;
        if(count_valid == label){
          return index;
        }
      } else if(empty_slot < 0){
        empty_slot = index;
      }
    }
    if(empty_slot >= 0){
      return empty_slot;
    }
    return -1;
  }
}; 


// Ensure buffer contains only valid hostname characters.
void sanitizeHostname(char* buffer);

void SetHostname(const char* new_hostname);

// The URL to an HTTP server where firmware can be pulled from.
void SetFirmwareServer(const char* new_fws, char* dest_buffer);

//bool pullFile(const String& filename);
bool enterList(String& input, bool& inside_list, int& list_index,
               int& current_level);
bool enterSubSet(String& input, int& level);
bool getKeyValue(String& input, String& key, String& value);

#endif  // ESP8266__HOST_ATTRIBUTES_H
