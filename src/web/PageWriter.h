#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace web {

// Streams an HTTP response in chunks through a small fixed buffer. Pages
// are written with the usual print()/printf() calls and leave as they are
// produced, so no page is ever assembled whole in RAM - which is what used
// to fragment the heap with multi-kilobyte Strings.
//
// Any extra header (sendHeader) must be queued BEFORE constructing the
// writer: the constructor sends the status line and the headers. The
// response is closed by finish() or by the destructor.
class PageWriter : public Print {
 public:
  PageWriter(WebServer &server, const char *contentType);
  ~PageWriter() { finish(); }

  size_t write(uint8_t c) override;
  size_t write(const uint8_t *data, size_t size) override;
  using Print::write;

  void finish();

 private:
  void sendBuffer();

  WebServer &server_;
  char       buffer_[768];
  size_t     length_   = 0;
  bool       finished_ = false;
};

}  // namespace web
