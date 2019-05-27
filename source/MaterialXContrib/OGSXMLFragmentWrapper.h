#ifndef MATERIALX_OGSX_ML_FRAGMENT_WRAPPER_H
#define MATERIALX_OGSX_ML_FRAGMENT_WRAPPER_H

/// @file
/// Contribution utility methods

#include <MaterialXGenShader/Library.h>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Element.h>

namespace MaterialX
{
/// @class OGSXMLFragmentWrapper
/// Class representing an OGS XML Fragment Wrapper
/// Interfaces allow the wrapper to be generated based on an input node and generator
/// The wrapper can contain fragment descriptions for more than one node if desired.
class OGSXMLFragmentWrapper
{
  public:
    OGSXMLFragmentWrapper();
    ~OGSXMLFragmentWrapper();

    /// Add a fragment wrapper from a node definition
    void createWrapperFromNode(NodePtr node, std::vector<GenContext*> contexts);
    /// Add a fragment wrapper for a shader generated from a given node for a given context
    void createWrapperFromShader(NodePtr node, GenContext& context);

    /// Get the contents of the cached XML document as a stream.
    void getDocument(std::ostream& stream);

  protected:
    StringMap _typeMap;

    void* _xmlDocument;
};

} // namespace MaterialX

#endif
