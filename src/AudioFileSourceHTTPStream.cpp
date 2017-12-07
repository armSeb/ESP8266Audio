/*
  AudioFileSourceHTTPStream
  Streaming HTTP source
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AudioFileSourceHTTPStream.h"

AudioFileSourceHTTPStream::AudioFileSourceHTTPStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL = NULL;
}

AudioFileSourceHTTPStream::AudioFileSourceHTTPStream(const char *url)
{
  saveURL = NULL;
  reconnectTries = 0;
  open(url);
}

bool AudioFileSourceHTTPStream::open(const char *url)
{
  pos = 0;
  http.begin(url);
  http.setReuse(true);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    cb.st(STATUS_HTTPFAIL, "Can't open HTTP request");
    return false;
  }
  size = http.getSize();
  free(saveURL);
  saveURL = strdup(url);
  return true;
}

AudioFileSourceHTTPStream::~AudioFileSourceHTTPStream()
{
  http.end();
}

uint32_t AudioFileSourceHTTPStream::read(void *data, uint32_t len)
{
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHTTPStream::readNonBlock(void *data, uint32_t len)
{
  return readInternal(data, len, true);
}

uint32_t AudioFileSourceHTTPStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, "Stream disconnected");
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[32];
      sprintf(buff, "Attempting to reconnect, try %d", i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, "Stream reconnected");
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, "Unable to reconnect");
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    cb.st(STATUS_NODATA, "No stream data available");
    http.end();
    goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = stream->readBytes(reinterpret_cast<uint8_t*>(data), len);
  pos += read;
  return read;
}

bool AudioFileSourceHTTPStream::seek(int32_t pos, int dir)
{
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceHTTPStream::close()
{
  http.end();
  return true;
}

bool AudioFileSourceHTTPStream::isOpen()
{
  return http.connected();
}

uint32_t AudioFileSourceHTTPStream::getSize()
{
  return size;
}

uint32_t AudioFileSourceHTTPStream::getPos()
{
  return pos;
}
