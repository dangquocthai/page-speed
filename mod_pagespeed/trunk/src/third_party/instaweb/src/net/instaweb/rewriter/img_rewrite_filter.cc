// Copyright 2010 Google Inc. All Rights Reserved.
// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/img_rewrite_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/img_filter.h"
#include "net/instaweb/rewriter/public/input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include <string>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/string_writer.h"
#define PAGESPEED_PNG_OPTIMIZER_GIF_READER 0
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"

namespace net_instaweb {

ImgRewriteFilter::ImgRewriteFilter(StringPiece path_prefix,
                                   HtmlParse* html_parse,
                                   ResourceManager* resource_manager)
    : RewriteFilter(path_prefix),
      html_parse_(html_parse),
      img_filter_(new ImgFilter(html_parse)),
      resource_manager_(resource_manager),
      s_width_(html_parse->Intern("width")),
      s_height_(html_parse->Intern("height")) {
}

// Intent: call this to actually create a new output resource after doing
// in-memory image recompression.  It will attempt to write the resource
// out and update the HtmlElement src attribute if it succeeds.
void ImgRewriteFilter::WriteBytesWithExtension(
    const ContentType& content_type, const std::string& contents,
    HtmlElement::Attribute* src) {
  MessageHandler* message_handler = html_parse_->message_handler();
  OutputResource* output_image =
      resource_manager_->GenerateOutputResource(content_type);
  bool written = output_image->StartWrite(message_handler);
  if (written) {
    written = output_image->WriteChunk(contents.data(), contents.size(),
                                       message_handler);
  }
  if (written) {
    written = output_image->EndWrite(message_handler);
  }
  if (written && output_image->IsReadable()) {
    // Success!  Rewrite img src attribute.  Log *first* to avoid
    // memory management trouble with old url string.
    const char* url = output_image->url().c_str();
    html_parse_->Info(src->value(), 0, "Remapped to %s", url);
    src->set_value(url);
  }
}

void ImgRewriteFilter::OptimizePng(
    pagespeed::image_compression::PngReaderInterface* reader,
    HtmlElement::Attribute* src, InputResource* img_resource) {
  std::string optimized_contents;
  if (pagespeed::image_compression::PngOptimizer::OptimizePng(
          *reader, img_resource->contents(), &optimized_contents)) {
    WriteBytesWithExtension(kContentTypePng, optimized_contents, src);
  }
}

void ImgRewriteFilter::OptimizeJpeg(HtmlElement::Attribute* src,
                                    InputResource* img_resource) {
  std::string optimized_contents;
  if (pagespeed::image_compression::OptimizeJpeg(
          img_resource->contents(), &optimized_contents)) {
    WriteBytesWithExtension(kContentTypeJpeg, optimized_contents, src);
  }
}

void ImgRewriteFilter::OptimizeImgResource(HtmlElement::Attribute* src,
                                           InputResource* img_resource) {
  switch (img_resource->image_type()) {
    case InputResource::IMAGE_JPEG: {
      OptimizeJpeg(src, img_resource);
      break;
    }
    case InputResource::IMAGE_PNG: {
      pagespeed::image_compression::PngReader png_reader;
      OptimizePng(&png_reader, src, img_resource);
      break;
    }
    case InputResource::IMAGE_GIF: {
#if PAGESPEED_PNG_OPTIMIZER_GIF_READER
      pagespeed::image_compression::GifReader gif_reader;
      OptimizePng(&gif_reader, src, img_resource);
#endif
      break;
    }
    case InputResource::IMAGE_UNKNOWN: {
      // If we don't recognize the resource, pass it through unchanged.
      html_parse_->Info(img_resource->url().c_str(), 0,
                        "Can't recognize image format");
      break;
    }
  }
}

void ImgRewriteFilter::EndElement(HtmlElement* element) {
  HtmlElement::Attribute *src = img_filter_->ParseImgElement(element);
  if (src != NULL) {
    // We now know that element is an img tag.
    // Log the element in its original form.
    // TODO(jmaessen): remove after initial debug?
    std::string tagstring;
    element->ToString(&tagstring);
    html_parse_->Info(
        html_parse_->filename(), element->begin_line_number(),
        "Found image: %s", tagstring.c_str());

    // Load img file and log its metadata.
    // TODO(jmaessen): right now loading synchronously.
    // Load asynchronously; cf css_combine_filter with
    // same TODO.  Plan: first resource request
    // initiates async fetch, fails, but populates resources
    // as they arrive so future requests succeed.
    InputResource* img_resource =
        resource_manager_->CreateInputResource(src->value());
    MessageHandler* message_handler = html_parse_->message_handler();
    if ((img_resource != NULL) && img_resource->Read(message_handler)) {
      if (img_resource->ContentsValid()) {
        OptimizeImgResource(src, img_resource);
      } else {
        html_parse_->Warning(img_resource->url().c_str(), 0,
                             "Img contents are invalid.");
      }
    } else {
      html_parse_->Warning(src->value(), 0, "Img contents weren't loaded");
    }
  }
}

void ImgRewriteFilter::Flush() {
  // TODO(jmaessen): wait here for resources to have been rewritten??
}

bool ImgRewriteFilter::Fetch(StringPiece resource_url,
                             Writer* writer,
                             const MetaData& request_header,
                             MetaData* response_headers,
                             UrlAsyncFetcher* fetcher,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  return false;
}

}  // namespace net_instaweb
