#include <xml_node.h>

static ID decorate, decorate_bang;

#ifdef DEBUG
static void debug_node_dealloc(xmlNodePtr x)
{
  NOKOGIRI_DEBUG_START(x)
  NOKOGIRI_DEBUG_END(x)
}
#else
#  define debug_node_dealloc 0
#endif

static void mark(xmlNodePtr node)
{
  // it's OK if the document isn't fully realized (as in XML::Reader).
  // see http://github.com/tenderlove/nokogiri/issues/closed/#issue/95
  if (DOC_RUBY_OBJECT_TEST(node->doc) && DOC_RUBY_OBJECT(node->doc))
    rb_gc_mark(DOC_RUBY_OBJECT(node->doc));
}

/* :nodoc: */
typedef xmlNodePtr (*node_other_func)(xmlNodePtr, xmlNodePtr);

/* :nodoc: */
static void relink_namespace(xmlNodePtr reparented)
{
  // Avoid segv when relinking against unlinked nodes.
  if(!reparented->parent) return;

  // Make sure that our reparented node has the correct namespaces
  if(!reparented->ns && reparented->doc != (xmlDocPtr)reparented->parent)
    xmlSetNs(reparented, reparented->parent->ns);

  // Search our parents for an existing definition
  if(reparented->nsDef) {
    xmlNsPtr ns = xmlSearchNsByHref(
        reparented->doc,
        reparented->parent,
        reparented->nsDef->href
    );
    if(ns && ns != reparented->nsDef) reparented->nsDef = NULL;
  }

  // Only walk all children if there actually is a namespace we need to
  // reparent.
  if(NULL == reparented->ns) return;

  // When a node gets reparented, walk it's children to make sure that
  // their namespaces are reparented as well.
  xmlNodePtr child = reparented->children;
  while(NULL != child) {
    relink_namespace(child);
    child = child->next;
  }
}

/* :nodoc: */
static xmlNodePtr xmlReplaceNodeWrapper(xmlNodePtr old, xmlNodePtr cur)
{
  xmlNodePtr retval ;
  retval = xmlReplaceNode(old, cur) ;
  if (retval == old) {
    return cur ; // return semantics for reparent_node_with
  }
  return retval ;
}

/* :nodoc: */
static VALUE reparent_node_with(VALUE node_obj, VALUE other_obj, node_other_func func)
{
  VALUE reparented_obj ;
  xmlNodePtr node, other, reparented ;

  if(!rb_obj_is_kind_of(node_obj, cNokogiriXmlNode))
    rb_raise(rb_eArgError, "node must be a Nokogiri::XML::Node");

  Data_Get_Struct(node_obj, xmlNode, node);
  Data_Get_Struct(other_obj, xmlNode, other);

  if(XML_DOCUMENT_NODE == node->type || XML_HTML_DOCUMENT_NODE == node->type)
    rb_raise(rb_eArgError, "cannot reparent a document node");

  if(node->type == XML_TEXT_NODE) {
    NOKOGIRI_ROOT_NODE(node);
    node = xmlDocCopyNode(node, other->doc, 1);
  }

  if (node->doc == other->doc) {
    xmlUnlinkNode(node) ;

    // TODO: I really want to remove this.  We shouldn't support 2.6.16 anymore
    if ( node->type == XML_TEXT_NODE
         && other->type == XML_TEXT_NODE
         && is_2_6_16() ) {

      // we'd rather leak than segfault.
      other->content = xmlStrdup(other->content);

    }

    if(!(reparented = (*func)(other, node))) {
      rb_raise(rb_eRuntimeError, "Could not reparent node (1)");
    }
  } else {
    xmlNodePtr duped_node ;
    // recursively copy to the new document
    if (!(duped_node = xmlDocCopyNode(node, other->doc, 1))) {
      rb_raise(rb_eRuntimeError, "Could not reparent node (xmlDocCopyNode)");
    }
    if(!(reparented = (*func)(other, duped_node))) {
      rb_raise(rb_eRuntimeError, "Could not reparent node (2)");
    }
    xmlUnlinkNode(node);
    NOKOGIRI_ROOT_NODE(node);
  }

  // the child was a text node that was coalesced. we need to have the object
  // point at SOMETHING, or we'll totally bomb out.
  if (reparented != node) {
    DATA_PTR(node_obj) = reparented ;
  }

  // Appropriately link in namespaces
  relink_namespace(reparented);

  reparented_obj = Nokogiri_wrap_xml_node(Qnil, reparented);

  rb_funcall(reparented_obj, decorate_bang, 0);

  return reparented_obj ;
}


/*
 * call-seq:
 *  document
 *
 * Get the document for this Node
 */
static VALUE document(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  return DOC_RUBY_OBJECT(node->doc);
}

/*
 * call-seq:
 *  pointer_id
 *
 * Get the internal pointer number
 */
static VALUE pointer_id(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);

  return INT2NUM((long)(node));
}

/*
 * call-seq:
 *  encode_special_chars(string)
 *
 * Encode any special characters in +string+
 */
static VALUE encode_special_chars(VALUE self, VALUE string)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  xmlChar * encoded = xmlEncodeSpecialChars(
      node->doc,
      (const xmlChar *)StringValuePtr(string)
  );

  VALUE encoded_str = NOKOGIRI_STR_NEW2(encoded);
  xmlFree(encoded);

  return encoded_str;
}

/*
 * call-seq:
 *  create_internal_subset(name, external_id, system_id)
 *
 * Create an internal subset
 */
static VALUE create_internal_subset(VALUE self, VALUE name, VALUE external_id, VALUE system_id)
{
  xmlNodePtr node;
  xmlDocPtr doc;
  Data_Get_Struct(self, xmlNode, node);

  doc = node->doc;

  if(xmlGetIntSubset(doc))
    rb_raise(rb_eRuntimeError, "Document already has an internal subset");

  xmlDtdPtr dtd = xmlCreateIntSubset(
      doc, 
      NIL_P(name)        ? NULL : (const xmlChar *)StringValuePtr(name),
      NIL_P(external_id) ? NULL : (const xmlChar *)StringValuePtr(external_id),
      NIL_P(system_id)   ? NULL : (const xmlChar *)StringValuePtr(system_id)
  );

  if(!dtd) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)dtd);
}

/*
 * call-seq:
 *  create_external_subset(name, external_id, system_id)
 *
 * Create an external subset
 */
static VALUE create_external_subset(VALUE self, VALUE name, VALUE external_id, VALUE system_id)
{
  xmlNodePtr node;
  xmlDocPtr doc;
  Data_Get_Struct(self, xmlNode, node);

  doc = node->doc;

  if(doc->extSubset)
    rb_raise(rb_eRuntimeError, "Document already has an external subset");

  xmlDtdPtr dtd = xmlNewDtd(
      doc, 
      NIL_P(name)        ? NULL : (const xmlChar *)StringValuePtr(name),
      NIL_P(external_id) ? NULL : (const xmlChar *)StringValuePtr(external_id),
      NIL_P(system_id)   ? NULL : (const xmlChar *)StringValuePtr(system_id)
  );

  if(!dtd) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)dtd);
}

/*
 * call-seq:
 *  external_subset
 *
 * Get the external subset
 */
static VALUE external_subset(VALUE self)
{
  xmlNodePtr node;
  xmlDocPtr doc;
  Data_Get_Struct(self, xmlNode, node);

  if(!node->doc) return Qnil;

  doc = node->doc;
  xmlDtdPtr dtd = doc->extSubset;

  if(!dtd) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)dtd);
}

/*
 * call-seq:
 *  internal_subset
 *
 * Get the internal subset
 */
static VALUE internal_subset(VALUE self)
{
  xmlNodePtr node;
  xmlDocPtr doc;
  Data_Get_Struct(self, xmlNode, node);

  if(!node->doc) return Qnil;

  doc = node->doc;
  xmlDtdPtr dtd = xmlGetIntSubset(doc);

  if(!dtd) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)dtd);
}

/*
 * call-seq:
 *  dup
 *
 * Copy this node.  An optional depth may be passed in, but it defaults
 * to a deep copy.  0 is a shallow copy, 1 is a deep copy.
 */
static VALUE duplicate_node(int argc, VALUE *argv, VALUE self)
{
  VALUE level;

  if(rb_scan_args(argc, argv, "01", &level) == 0)
    level = INT2NUM((long)1);

  xmlNodePtr node, dup;
  Data_Get_Struct(self, xmlNode, node);

  dup = xmlDocCopyNode(node, node->doc, (int)NUM2INT(level));
  if(dup == NULL) return Qnil;

  return Nokogiri_wrap_xml_node(rb_obj_class(self), dup);
}

/*
 * call-seq:
 *  unlink
 *
 * Unlink this node from its current context.
 */
static VALUE unlink_node(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  xmlUnlinkNode(node);
  NOKOGIRI_ROOT_NODE(node);
  return self;
}

/*
 * call-seq:
 *  blank?
 *
 * Is this node blank?
 */
static VALUE blank_eh(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  if(1 == xmlIsBlankNode(node))
    return Qtrue;
  return Qfalse;
}

/*
 * call-seq:
 *  next_sibling
 *
 * Returns the next sibling node
 */
static VALUE next_sibling(VALUE self)
{
  xmlNodePtr node, sibling;
  Data_Get_Struct(self, xmlNode, node);

  sibling = node->next;
  if(!sibling) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, sibling) ;
}

/*
 * call-seq:
 *  previous_sibling
 *
 * Returns the previous sibling node
 */
static VALUE previous_sibling(VALUE self)
{
  xmlNodePtr node, sibling;
  Data_Get_Struct(self, xmlNode, node);

  sibling = node->prev;
  if(!sibling) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, sibling);
}

/*
 * call-seq:
 *  next_element
 *
 * Returns the next Nokogiri::XML::Element type sibling node.
 */
static VALUE next_element(VALUE self)
{
  xmlNodePtr node, sibling;
  Data_Get_Struct(self, xmlNode, node);

  sibling = node->next;
  if(!sibling) return Qnil;

  while(sibling && sibling->type != XML_ELEMENT_NODE)
    sibling = sibling->next;

  return sibling ? Nokogiri_wrap_xml_node(Qnil, sibling) : Qnil ;
}

/*
 * call-seq:
 *  previous_element
 *
 * Returns the previous Nokogiri::XML::Element type sibling node.
 */
static VALUE previous_element(VALUE self)
{
  xmlNodePtr node, sibling;
  Data_Get_Struct(self, xmlNode, node);

  sibling = node->prev;
  if(!sibling) return Qnil;

  while(sibling && sibling->type != XML_ELEMENT_NODE)
    sibling = sibling->prev;

  return sibling ? Nokogiri_wrap_xml_node(Qnil, sibling) : Qnil ;
}

/* :nodoc: */
static VALUE replace(VALUE self, VALUE _new_node)
{
  reparent_node_with(_new_node, self, xmlReplaceNodeWrapper) ;
  return self ;
}

/*
 * call-seq:
 *  children
 *
 * Get the list of children for this node as a NodeSet
 */
static VALUE children(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);

  xmlNodePtr child = node->children;
  xmlNodeSetPtr set = xmlXPathNodeSetCreate(child);

  if(!child) return Nokogiri_wrap_xml_node_set(set);

  child = child->next;
  while(NULL != child) {
    xmlXPathNodeSetAdd(set, child);
    child = child->next;
  }

  VALUE node_set = Nokogiri_wrap_xml_node_set(set);
  rb_iv_set(node_set, "@document", DOC_RUBY_OBJECT(node->doc));

  return node_set;
}

/*
 * call-seq:
 *  child
 *
 * Returns the child node
 */
static VALUE child(VALUE self)
{
  xmlNodePtr node, child;
  Data_Get_Struct(self, xmlNode, node);

  child = node->children;
  if(!child) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, child);
}

/*
 * call-seq:
 *  key?(attribute)
 *
 * Returns true if +attribute+ is set
 */
static VALUE key_eh(VALUE self, VALUE attribute)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  if(xmlHasProp(node, (xmlChar *)StringValuePtr(attribute)))
    return Qtrue;
  return Qfalse;
}

/*
 * call-seq:
 *  namespaced_key?(attribute, namespace)
 *
 * Returns true if +attribute+ is set with +namespace+
 */
static VALUE namespaced_key_eh(VALUE self, VALUE attribute, VALUE namespace)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  if(xmlHasNsProp(node, (xmlChar *)StringValuePtr(attribute),
        NIL_P(namespace) ? NULL : (xmlChar *)StringValuePtr(namespace)))
    return Qtrue;
  return Qfalse;
}

/*
 * call-seq:
 *  []=(property, value)
 *
 * Set the +property+ to +value+
 */
static VALUE set(VALUE self, VALUE property, VALUE value)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);

  xmlSetProp(node, (xmlChar *)StringValuePtr(property),
      (xmlChar *)StringValuePtr(value));

  return value;
}

/*
 * call-seq:
 *   get(attribute)
 *
 * Get the value for +attribute+
 */
static VALUE get(VALUE self, VALUE attribute)
{
  xmlNodePtr node;
  xmlChar* propstr ;
  VALUE rval ;
  Data_Get_Struct(self, xmlNode, node);

  if(NIL_P(attribute)) return Qnil;

  propstr = xmlGetProp(node, (xmlChar *)StringValuePtr(attribute));

  if(!propstr) return Qnil;

  rval = NOKOGIRI_STR_NEW2(propstr);

  xmlFree(propstr);
  return rval ;
}

/*
 * call-seq:
 *   set_namespace(namespace)
 *
 * Set the namespace to +namespace+
 */
static VALUE set_namespace(VALUE self, VALUE namespace)
{
  xmlNodePtr node;
  xmlNsPtr ns;

  Data_Get_Struct(self, xmlNode, node);
  Data_Get_Struct(namespace, xmlNs, ns);

  xmlSetNs(node, ns);

  return self;
}

/*
 * call-seq:
 *   attribute(name)
 *
 * Get the attribute node with +name+
 */
static VALUE attr(VALUE self, VALUE name)
{
  xmlNodePtr node;
  xmlAttrPtr prop;
  Data_Get_Struct(self, xmlNode, node);
  prop = xmlHasProp(node, (xmlChar *)StringValuePtr(name));

  if(! prop) return Qnil;
  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)prop);
}

/*
 * call-seq:
 *   attribute_with_ns(name, namespace)
 *
 * Get the attribute node with +name+ and +namespace+
 */
static VALUE attribute_with_ns(VALUE self, VALUE name, VALUE namespace)
{
  xmlNodePtr node;
  xmlAttrPtr prop;
  Data_Get_Struct(self, xmlNode, node);
  prop = xmlHasNsProp(node, (xmlChar *)StringValuePtr(name),
      NIL_P(namespace) ? NULL : (xmlChar *)StringValuePtr(namespace));

  if(! prop) return Qnil;
  return Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)prop);
}

/*
 *  call-seq:
 *    attribute_nodes()
 *
 *  returns a list containing the Node attributes.
 */
static VALUE attribute_nodes(VALUE self)
{
    /* this code in the mode of xmlHasProp() */
    xmlNodePtr node;

    Data_Get_Struct(self, xmlNode, node);

    VALUE attr = rb_ary_new();
    Nokogiri_xml_node_properties(node, attr);

    return attr ;
}


/*
 *  call-seq:
 *    namespace()
 *
 *  returns the Nokogiri::XML::Namespace for the node, if one exists.
 */
static VALUE namespace(VALUE self)
{
  xmlNodePtr node ;
  Data_Get_Struct(self, xmlNode, node);

  if (node->ns)
    return Nokogiri_wrap_xml_namespace(node->doc, node->ns);

  return Qnil ;
}

/*
 *  call-seq:
 *    namespace_definitions()
 *
 *  returns a list of Namespace nodes defined on _self_
 */
static VALUE namespace_definitions(VALUE self)
{
  /* this code in the mode of xmlHasProp() */
  xmlNodePtr node ;

  Data_Get_Struct(self, xmlNode, node);

  VALUE list = rb_ary_new();

  xmlNsPtr ns = node->nsDef;

  if(!ns) return list;

  while(NULL != ns) {
    rb_ary_push(list, Nokogiri_wrap_xml_namespace(node->doc, ns));
    ns = ns->next;
  }

  return list;
}

/*
 * call-seq:
 *  node_type
 *
 * Get the type for this Node
 */
static VALUE node_type(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  return INT2NUM((long)node->type);
}

/*
 * call-seq:
 *  content=
 *
 * Set the content for this Node
 */
static VALUE set_content(VALUE self, VALUE content)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  xmlNodeSetContent(node, (xmlChar *)StringValuePtr(content));
  return content;
}

/*
 * call-seq:
 *  content
 *
 * Returns the content for this Node
 */
static VALUE get_content(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);

  xmlChar * content = xmlNodeGetContent(node);
  if(content) {
    VALUE rval = NOKOGIRI_STR_NEW2(content);
    xmlFree(content);
    return rval;
  }
  return Qnil;
}

/* :nodoc: */
static VALUE add_child(VALUE self, VALUE child)
{
  return reparent_node_with(child, self, xmlAddChild);
}

/*
 * call-seq:
 *  parent
 *
 * Get the parent Node for this Node
 */
static VALUE get_parent(VALUE self)
{
  xmlNodePtr node, parent;
  Data_Get_Struct(self, xmlNode, node);

  parent = node->parent;
  if(!parent) return Qnil;

  return Nokogiri_wrap_xml_node(Qnil, parent) ;
}

/*
 * call-seq:
 *  name=(new_name)
 *
 * Set the name for this Node
 */
static VALUE set_name(VALUE self, VALUE new_name)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  xmlNodeSetName(node, (xmlChar*)StringValuePtr(new_name));
  return new_name;
}

/*
 * call-seq:
 *  name
 *
 * Returns the name for this Node
 */
static VALUE get_name(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);
  if(node->name)
    return NOKOGIRI_STR_NEW2(node->name);
  return Qnil;
}

/*
 * call-seq:
 *  path
 *
 * Returns the path associated with this Node
 */
static VALUE path(VALUE self)
{
  xmlNodePtr node;
  xmlChar *path ;
  Data_Get_Struct(self, xmlNode, node);
  
  path = xmlGetNodePath(node);
  VALUE rval = NOKOGIRI_STR_NEW2(path);
  xmlFree(path);
  return rval ;
}

/* :nodoc: */
static VALUE add_next_sibling(VALUE self, VALUE rb_node)
{
  return reparent_node_with(rb_node, self, xmlAddNextSibling) ;
}

/* :nodoc: */
static VALUE add_previous_sibling(VALUE self, VALUE rb_node)
{
  return reparent_node_with(rb_node, self, xmlAddPrevSibling) ;
}

/*
 * call-seq:
 *  native_write_to(io, encoding, options)
 *
 * Write this Node to +io+ with +encoding+ and +options+
 */
static VALUE native_write_to(
    VALUE self,
    VALUE io,
    VALUE encoding,
    VALUE indent_string,
    VALUE options
) {
  xmlNodePtr node;

  Data_Get_Struct(self, xmlNode, node);

  xmlIndentTreeOutput = 1;

  const char * before_indent = xmlTreeIndentString;

  xmlTreeIndentString = StringValuePtr(indent_string);

  xmlSaveCtxtPtr savectx = xmlSaveToIO(
      (xmlOutputWriteCallback)io_write_callback,
      (xmlOutputCloseCallback)io_close_callback,
      (void *)io,
      RTEST(encoding) ? StringValuePtr(encoding) : NULL,
      (int)NUM2INT(options)
  );

  xmlSaveTree(savectx, node);
  xmlSaveClose(savectx);

  xmlTreeIndentString = before_indent;
  return io;
}

/*
 * call-seq:
 *  line
 *
 * Returns the line for this Node
 */
static VALUE line(VALUE self)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);

  return INT2NUM(xmlGetLineNo(node));
}

/*
 * call-seq:
 *  add_namespace_definition(prefix, href)
 *
 * Adds a namespace definition with +prefix+ using +href+
 */
static VALUE add_namespace_definition(VALUE self, VALUE prefix, VALUE href)
{
  xmlNodePtr node;
  Data_Get_Struct(self, xmlNode, node);


  xmlNsPtr ns = xmlNewNs(
      node,
      (const xmlChar *)StringValuePtr(href),
      (const xmlChar *)(NIL_P(prefix) ? NULL : StringValuePtr(prefix))
  );

  if(!ns) {
    ns = xmlSearchNs(
        node->doc,
        node,
        (const xmlChar *)(NIL_P(prefix) ? NULL : StringValuePtr(prefix))
    );
  }

  if(NIL_P(prefix)) xmlSetNs(node, ns);

  return Nokogiri_wrap_xml_namespace(node->doc, ns);
}

/*
 * call-seq:
 *   new(name, document)
 *
 * Create a new node with +name+ sharing GC lifecycle with +document+
 */
static VALUE new(int argc, VALUE *argv, VALUE klass)
{
  xmlDocPtr doc;
  VALUE name;
  VALUE document;
  VALUE rest;

  rb_scan_args(argc, argv, "2*", &name, &document, &rest);

  Data_Get_Struct(document, xmlDoc, doc);

  xmlNodePtr node = xmlNewNode(NULL, (xmlChar *)StringValuePtr(name));
  node->doc = doc->doc;
  NOKOGIRI_ROOT_NODE(node);

  VALUE rb_node = Nokogiri_wrap_xml_node(
      klass == cNokogiriXmlNode ? (VALUE)NULL : klass,
      node
  );
  rb_obj_call_init(rb_node, argc, argv);

  if(rb_block_given_p()) rb_yield(rb_node);

  return rb_node;
}

/*
 * call-seq:
 *  dump_html
 *
 * Returns the Node as html.
 */
static VALUE dump_html(VALUE self)
{
  xmlBufferPtr buf ;
  xmlNodePtr node ;
  Data_Get_Struct(self, xmlNode, node);

  buf = xmlBufferCreate() ;
  htmlNodeDump(buf, node->doc, node);
  VALUE html = NOKOGIRI_STR_NEW2(buf->content);
  xmlBufferFree(buf);
  return html ;
}

/*
 * call-seq:
 *  compare(other)
 *
 * Compare this Node to +other+ with respect to their Document
 */
static VALUE compare(VALUE self, VALUE _other)
{
  xmlNodePtr node, other;
  Data_Get_Struct(self, xmlNode, node);
  Data_Get_Struct(_other, xmlNode, other);

  return INT2NUM((long)xmlXPathCmpNodes(other, node));
}

VALUE Nokogiri_wrap_xml_node(VALUE klass, xmlNodePtr node)
{
  assert(node);

  VALUE document = Qnil ;
  VALUE node_cache = Qnil ;
  VALUE rb_node = Qnil ;

  if(node->type == XML_DOCUMENT_NODE || node->type == XML_HTML_DOCUMENT_NODE)
      return DOC_RUBY_OBJECT(node->doc);

  if(NULL != node->_private) return (VALUE)node->_private;

  if(RTEST(klass))
    rb_node = Data_Wrap_Struct(klass, mark, debug_node_dealloc, node) ;

  else switch(node->type)
  {
    case XML_ELEMENT_NODE:
      klass = cNokogiriXmlElement;
      break;
    case XML_TEXT_NODE:
      klass = cNokogiriXmlText;
      break;
    case XML_ATTRIBUTE_NODE:
      klass = cNokogiriXmlAttr;
      break;
    case XML_ENTITY_REF_NODE:
      klass = cNokogiriXmlEntityReference;
      break;
    case XML_COMMENT_NODE:
      klass = cNokogiriXmlComment;
      break;
    case XML_DOCUMENT_FRAG_NODE:
      klass = cNokogiriXmlDocumentFragment;
      break;
    case XML_PI_NODE:
      klass = cNokogiriXmlProcessingInstruction;
      break;
    case XML_ENTITY_DECL:
      klass = cNokogiriXmlEntityDecl;
      break;
    case XML_CDATA_SECTION_NODE:
      klass = cNokogiriXmlCData;
      break;
    case XML_DTD_NODE:
      klass = cNokogiriXmlDtd;
      break;
    case XML_ATTRIBUTE_DECL:
      klass = cNokogiriXmlAttributeDecl;
      break;
    case XML_ELEMENT_DECL:
      klass = cNokogiriXmlElementDecl;
      break;
    default:
      klass = cNokogiriXmlNode;
  }

  rb_node = Data_Wrap_Struct(klass, mark, debug_node_dealloc, node) ;

  node->_private = (void *)rb_node;

  if (DOC_RUBY_OBJECT_TEST(node->doc) && DOC_RUBY_OBJECT(node->doc)) {
    // it's OK if the document isn't fully realized (as in XML::Reader).
    // see http://github.com/tenderlove/nokogiri/issues/closed/#issue/95
    document = DOC_RUBY_OBJECT(node->doc);
    node_cache = DOC_NODE_CACHE(node->doc);
    rb_ary_push(node_cache, rb_node);
    rb_funcall(document, decorate, 1, rb_node);
  }

  return rb_node ;
}


void Nokogiri_xml_node_properties(xmlNodePtr node, VALUE attr_list)
{
  xmlAttrPtr prop;
  prop = node->properties ;
  while (prop != NULL) {
    rb_ary_push(attr_list, Nokogiri_wrap_xml_node(Qnil, (xmlNodePtr)prop));
    prop = prop->next ;
  }
}

VALUE cNokogiriXmlNode ;
VALUE cNokogiriXmlElement ;

void init_xml_node()
{
  VALUE nokogiri = rb_define_module("Nokogiri");
  VALUE xml = rb_define_module_under(nokogiri, "XML");
  VALUE klass = rb_define_class_under(xml, "Node", rb_cObject);

  cNokogiriXmlNode = klass;

  cNokogiriXmlElement = rb_define_class_under(xml, "Element", klass);

  rb_define_singleton_method(klass, "new", new, -1);

  rb_define_method(klass, "add_namespace_definition", add_namespace_definition, 2);
  rb_define_method(klass, "node_name", get_name, 0);
  rb_define_method(klass, "document", document, 0);
  rb_define_method(klass, "node_name=", set_name, 1);
  rb_define_method(klass, "parent", get_parent, 0);
  rb_define_method(klass, "child", child, 0);
  rb_define_method(klass, "children", children, 0);
  rb_define_method(klass, "next_sibling", next_sibling, 0);
  rb_define_method(klass, "previous_sibling", previous_sibling, 0);
  rb_define_method(klass, "next_element", next_element, 0);
  rb_define_method(klass, "previous_element", previous_element, 0);
  rb_define_method(klass, "node_type", node_type, 0);
  rb_define_method(klass, "content", get_content, 0);
  rb_define_method(klass, "path", path, 0);
  rb_define_method(klass, "key?", key_eh, 1);
  rb_define_method(klass, "namespaced_key?", namespaced_key_eh, 2);
  rb_define_method(klass, "blank?", blank_eh, 0);
  rb_define_method(klass, "[]=", set, 2);
  rb_define_method(klass, "attribute_nodes", attribute_nodes, 0);
  rb_define_method(klass, "attribute", attr, 1);
  rb_define_method(klass, "attribute_with_ns", attribute_with_ns, 2);
  rb_define_method(klass, "namespace", namespace, 0);
  rb_define_method(klass, "namespace_definitions", namespace_definitions, 0);
  rb_define_method(klass, "encode_special_chars", encode_special_chars, 1);
  rb_define_method(klass, "dup", duplicate_node, -1);
  rb_define_method(klass, "unlink", unlink_node, 0);
  rb_define_method(klass, "internal_subset", internal_subset, 0);
  rb_define_method(klass, "external_subset", external_subset, 0);
  rb_define_method(klass, "create_internal_subset", create_internal_subset, 3);
  rb_define_method(klass, "create_external_subset", create_external_subset, 3);
  rb_define_method(klass, "pointer_id", pointer_id, 0);
  rb_define_method(klass, "line", line, 0);

  rb_define_private_method(klass, "add_child_node", add_child, 1);
  rb_define_private_method(klass, "add_previous_sibling_node", add_previous_sibling, 1);
  rb_define_private_method(klass, "add_next_sibling_node", add_next_sibling, 1);
  rb_define_private_method(klass, "replace_node", replace, 1);
  rb_define_private_method(klass, "dump_html", dump_html, 0);
  rb_define_private_method(klass, "native_write_to", native_write_to, 4);
  rb_define_private_method(klass, "native_content=", set_content, 1);
  rb_define_private_method(klass, "get", get, 1);
  rb_define_private_method(klass, "set_namespace", set_namespace, 1);
  rb_define_private_method(klass, "compare", compare, 1);

  decorate      = rb_intern("decorate");
  decorate_bang = rb_intern("decorate!");
}
