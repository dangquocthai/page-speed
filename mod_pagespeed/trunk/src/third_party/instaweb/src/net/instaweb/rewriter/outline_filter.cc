// Copyright 2010 and onwards Google Inc.
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/outline_filter.h"

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const char kTextCss[] = "text/css";
const char kTextJavascript[] = "text/javascript";
const char kStylesheet[] = "stylesheet";

OutlineFilter::OutlineFilter(HtmlParse* html_parse,
                             ResourceManager* resource_manager,
                             bool outline_styles,
                             bool outline_scripts)
    : inline_element_(NULL),
      html_parse_(html_parse),
      resource_manager_(resource_manager),
      outline_styles_(outline_styles),
      outline_scripts_(outline_scripts) {
  s_link_ = html_parse_->Intern("link");
  s_script_ = html_parse_->Intern("script");
  s_style_ = html_parse_->Intern("style");
  s_rel_ = html_parse_->Intern("rel");
  s_href_ = html_parse_->Intern("href");
  s_src_ = html_parse_->Intern("src");
  s_type_ = html_parse_->Intern("type");
}

void OutlineFilter::StartDocument() {
  inline_element_ = NULL;
  buffer_.clear();
}

void OutlineFilter::StartElement(HtmlElement* element) {
  // No tags allowed inside style or script element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    html_parse_->ErrorHere("Tag '%s' found inside style/script.",
                           element->tag().c_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
  if (outline_styles_ && element->tag() == s_style_) {
    inline_element_ = element;
    buffer_.clear();

  } else if (outline_scripts_ && element->tag() == s_script_) {
    inline_element_ = element;
    buffer_.clear();
    // script elements which already have a src should not be outlined.
    for (int i = 0; i < element->attribute_size(); ++i) {
      if (element->attribute(i).name() == s_src_) {
        inline_element_ = NULL;
      }
    }
  }
}

void OutlineFilter::EndElement(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside style or script element.
      html_parse_->ErrorHere("Tag '%s' found inside style/script.",
                             element->tag().c_str());

    } else if (inline_element_->tag() == s_style_) {
      OutlineStyle(inline_element_, buffer_);

    } else if (inline_element_->tag() == s_script_) {
      OutlineScript(inline_element_, buffer_);

    } else {
      html_parse_->ErrorHere("OutlineFilter::inline_element_ "
                             "Expected: 'style' or 'script', Actual: '%s'",
                             inline_element_->tag().c_str());
    }
    inline_element_ = NULL;
    buffer_.clear();
  }
}

void OutlineFilter::Flush() {
  // If we were flushed in a style/script element, we cannot outline it.
  inline_element_ = NULL;
  buffer_.clear();
}


void OutlineFilter::Characters(const std::string& characters) {
  if (inline_element_ != NULL) {
    buffer_ += characters;
  }
}

void OutlineFilter::IgnorableWhitespace(const std::string& whitespace) {
  if (inline_element_ != NULL) {
    buffer_ += whitespace;
  }
}


void OutlineFilter::Comment(const std::string& comment) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("Comment found inside style/script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void OutlineFilter::Cdata(const std::string& cdata) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("CDATA found inside style/script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void OutlineFilter::IEDirective(const std::string& directive) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("IE Directive found inside style/script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

// Try to write content and possibly header to resource.
bool OutlineFilter::WriteResource(const std::string& content,
                                  OutputResource* resource,
                                  MessageHandler* handler) {
  bool write_worked = resource->StartWrite(handler);
  write_worked &= resource->WriteChunk(content, handler);
  write_worked &= resource->EndWrite(handler);
  return write_worked;
}

// Create file with style content and remove that element from DOM.
void OutlineFilter::OutlineStyle(HtmlElement* style_element,
                                 const std::string& content) {
  if (html_parse_->IsRewritable(style_element)) {
    // Create style file from content.
    HtmlElement::Attribute* type =
        style_element->FirstAttributeWithName(s_type_);
    // We only deal with CSS styles.  If no type specified, CSS is assumed.
    // TODO(sligocki): Is this assumption appropriate?
    if (type == NULL || strcmp(type->value(), kTextCss) == 0) {
      OutputResource* resource =
          resource_manager_->CreateOutputResource(kContentTypeCss);
      MessageHandler* handler = html_parse_->message_handler();
      if (WriteResource(content, resource, handler)) {
        HtmlElement* link_element = html_parse_->NewElement(s_link_);
        link_element->AddAttribute(s_rel_, kStylesheet, "'");
        link_element->AddAttribute(s_href_, resource->url().c_str(), "'");
        // Add all style atrributes to link.
        for (int i = 0; i < style_element->attribute_size(); ++i) {
          const HtmlElement::Attribute& attr = style_element->attribute(i);
          link_element->AddAttribute(attr.name(), attr.value(), attr.quote());
        }
        // Remove style element from DOM.
        if (!html_parse_->DeleteElement(style_element)) {
          html_parse_->FatalErrorHere("Failed to delete element");
        }
        // Add link.
        // NOTE: this only works if current pointer was on element.
        // TODO(sligocki): Do an InsertElementBeforeElement instead?
        html_parse_->InsertElementBeforeCurrent(link_element);
      } else {
        html_parse_->ErrorHere("Failed to write style resource.");
      }
    } else {
      std::string element_string;
      style_element->ToString(&element_string);
      html_parse_->InfoHere("Cannot outline non-css stylesheet %s",
                           element_string.c_str());
    }
  }
}

// Create file with script content and remove that element from DOM.
// TODO(sligocki): Combine similar code from OutlineStyle.
void OutlineFilter::OutlineScript(HtmlElement* element,
                                  const std::string& content) {
  if (html_parse_->IsRewritable(element)) {
    // Create script file from content.
    HtmlElement::Attribute* type = element->FirstAttributeWithName(s_type_);
    // We only deal with javascript styles. If no type specified, JS is assumed.
    // TODO(sligocki): Is this assumption appropriate?
    if (type == NULL || strcmp(type->value(), kTextJavascript) == 0) {
      OutputResource* resource =
          resource_manager_->CreateOutputResource(kContentTypeJavascript);
      MessageHandler* handler = html_parse_->message_handler();
      if (WriteResource(content, resource, handler)) {
        HtmlElement* src_element = html_parse_->NewElement(s_script_);
        src_element->AddAttribute(s_src_, resource->url().c_str(), "'");
        // Add all atrributes from old script element to new script src element.
        for (int i = 0; i < element->attribute_size(); ++i) {
          const HtmlElement::Attribute& attr = element->attribute(i);
          src_element->AddAttribute(attr.name(), attr.value(), attr.quote());
        }
        // Remove original script element from DOM.
        if (!html_parse_->DeleteElement(element)) {
          html_parse_->FatalErrorHere("Failed to delete element");
        }
        // Add <script src=...> element.
        // NOTE: this only works if current pointer was on element.
        // TODO(sligocki): Do an InsertElementBeforeElement instead?
        html_parse_->InsertElementBeforeCurrent(src_element);
      } else {
        html_parse_->ErrorHere("Failed to write script resource.");
      }
    } else {
      std::string element_string;
      element->ToString(&element_string);
      html_parse_->InfoHere("Cannot outline non-javascript script %s",
                           element_string.c_str());
    }
  }
}

}  // namespace net_instaweb
