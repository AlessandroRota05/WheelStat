#include "PageWriter.h"

namespace web {

PageWriter::PageWriter(WebServer &server, const char *contentType) : server_(server) {
  // Unknown length = chunked transfer: each sendContent() is one chunk.
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, contentType, "");
}

void PageWriter::sendBuffer() {
  // Never an empty chunk here: in chunked transfer that is the terminator.
  if (length_ == 0) return;
  server_.sendContent(buffer_, length_);
  length_ = 0;
}

size_t PageWriter::write(uint8_t c) {
  if (length_ == sizeof(buffer_)) sendBuffer();
  buffer_[length_++] = (char)c;
  return 1;
}

size_t PageWriter::write(const uint8_t *data, size_t size) {
  if (size == 0) return 0;

  if (size > sizeof(buffer_) - length_) {
    sendBuffer();
    // Larger than the whole buffer (the static assets): straight out.
    if (size > sizeof(buffer_)) {
      server_.sendContent((const char *)data, size);
      return size;
    }
  }
  memcpy(buffer_ + length_, data, size);
  length_ += size;
  return size;
}

void PageWriter::finish() {
  if (finished_) return;
  finished_ = true;
  sendBuffer();
  server_.sendContent("");  // zero-length chunk closes the response
}

}  // namespace web
