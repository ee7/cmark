#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "cmark.h"
#include "node.h"
#include "buffer.h"
#include "houdini.h"

// Functions to convert cmark_nodes to XML strings.

static void escape_xml(cmark_strbuf *dest, const unsigned char *source,
                       bufsize_t length) {
  houdini_escape_html0(dest, source, length, 0);
}

struct render_state {
  cmark_strbuf *xml;
  int indent;
};

static inline void indent(struct render_state *state) {
  int i;
  for (i = 0; i < state->indent; i++) {
    cmark_strbuf_putc(state->xml, ' ');
  }
}

static int S_render_node(cmark_node *node, cmark_event_type ev_type,
                         struct render_state *state, int options) {
  cmark_strbuf *xml = state->xml;
  bool literal = false;
  cmark_delim_type delim;
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  char buffer[100];

  if (entering) {
    indent(state);
    cmark_strbuf_putc(xml, '<');
    cmark_strbuf_puts(xml, cmark_node_get_type_string(node));

    if (options & CMARK_OPT_SOURCEPOS && node->start_line != 0) {
      sprintf(buffer, " sourcepos=\"%d:%d-%d:%d\"", node->start_line,
              node->start_column, node->end_line, node->end_column);
      cmark_strbuf_puts(xml, buffer);
    }

    literal = false;

    switch (node->type) {
    case CMARK_NODE_TEXT:
    case CMARK_NODE_CODE:
    case CMARK_NODE_HTML:
    case CMARK_NODE_INLINE_HTML:
      cmark_strbuf_puts(xml, ">");
      escape_xml(xml, node->as.literal.data, node->as.literal.len);
      cmark_strbuf_puts(xml, "</");
      cmark_strbuf_puts(xml, cmark_node_get_type_string(node));
      literal = true;
      break;
    case CMARK_NODE_LIST:
      switch (cmark_node_get_list_type(node)) {
      case CMARK_ORDERED_LIST:
        cmark_strbuf_puts(xml, " type=\"ordered\"");
        sprintf(buffer, " start=\"%d\"", cmark_node_get_list_start(node));
        cmark_strbuf_puts(xml, buffer);
        delim = cmark_node_get_list_delim(node);
        if (delim == CMARK_PAREN_DELIM) {
          cmark_strbuf_puts(xml, " delim=\"paren\"");
        } else if (delim == CMARK_PERIOD_DELIM) {
          cmark_strbuf_puts(xml, " delim=\"period\"");
        }
        break;
      case CMARK_BULLET_LIST:
        cmark_strbuf_puts(xml, " type=\"bullet\"");
        break;
      default:
        break;
      }
      sprintf(buffer, " tight=\"%s\"",
              (cmark_node_get_list_tight(node) ? "true" : "false"));
      cmark_strbuf_puts(xml, buffer);
      break;
    case CMARK_NODE_HEADER:
      sprintf(buffer, " level=\"%d\"", node->as.header.level);
      cmark_strbuf_puts(xml, buffer);
      break;
    case CMARK_NODE_CODE_BLOCK:
      if (node->as.code.info.len > 0) {
        cmark_strbuf_puts(xml, " info=\"");
        escape_xml(xml, node->as.code.info.data, node->as.code.info.len);
        cmark_strbuf_putc(xml, '"');
      }
      cmark_strbuf_puts(xml, ">");
      escape_xml(xml, node->as.code.literal.data, node->as.code.literal.len);
      cmark_strbuf_puts(xml, "</");
      cmark_strbuf_puts(xml, cmark_node_get_type_string(node));
      literal = true;
      break;
    case CMARK_NODE_CUSTOM_BLOCK:
    case CMARK_NODE_CUSTOM_INLINE:
      cmark_strbuf_puts(xml, " on_enter=\"");
      escape_xml(xml, node->as.custom.on_enter.data, node->as.custom.on_enter.len);
      cmark_strbuf_putc(xml, '"');
      cmark_strbuf_puts(xml, " on_exit=\"");
      escape_xml(xml, node->as.custom.on_exit.data, node->as.custom.on_exit.len);
      cmark_strbuf_putc(xml, '"');
      break;
    case CMARK_NODE_LINK:
    case CMARK_NODE_IMAGE:
      cmark_strbuf_puts(xml, " destination=\"");
      escape_xml(xml, node->as.link.url.data, node->as.link.url.len);
      cmark_strbuf_putc(xml, '"');
      cmark_strbuf_puts(xml, " title=\"");
      escape_xml(xml, node->as.link.title.data, node->as.link.title.len);
      cmark_strbuf_putc(xml, '"');
      break;
    default:
      break;
    }
    if (node->first_child) {
      state->indent += 2;
    } else if (!literal) {
      cmark_strbuf_puts(xml, " /");
    }
    cmark_strbuf_puts(xml, ">\n");

  } else if (node->first_child) {
    state->indent -= 2;
    indent(state);
    cmark_strbuf_puts(xml, "</");
    cmark_strbuf_puts(xml, cmark_node_get_type_string(node));
    cmark_strbuf_puts(xml, ">\n");
  }

  return 1;
}

char *cmark_render_xml(cmark_node *root, int options) {
  char *result;
  cmark_strbuf xml = GH_BUF_INIT;
  cmark_event_type ev_type;
  cmark_node *cur;
  struct render_state state = {&xml, 0};

  cmark_iter *iter = cmark_iter_new(root);

  cmark_strbuf_puts(state.xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  cmark_strbuf_puts(state.xml,
                    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n");
  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    S_render_node(cur, ev_type, &state, options);
  }
  result = (char *)cmark_strbuf_detach(&xml);

  cmark_iter_free(iter);
  return result;
}
